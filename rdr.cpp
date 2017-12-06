/*
 * rdr.cpp
 */

#define UNICODE

#include <opencv2/opencv.hpp>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <cmath>
#include <thread>
#include "tclap/CmdLine.h"
#include <windows.h>


// ----------------------------------------------------------
// initialized data
const double dh_wait = 100; //seconds to wait for DrawingHand

const std::vector<int> OpenCV_JPEG_options = {CV_IMWRITE_JPEG_QUALITY,        100,
                                              CV_IMWRITE_JPEG_PROGRESSIVE,      0,
                                              CV_IMWRITE_JPEG_OPTIMIZE,         1,
                                              CV_IMWRITE_JPEG_RST_INTERVAL,     0,
                                              CV_IMWRITE_JPEG_LUMA_QUALITY,   100,
                                              CV_IMWRITE_JPEG_CHROMA_QUALITY, 100}; //cv::IMWRITE_JPEG_CHROMA_QUALITY

const char rdr_new_header[] = {'\x2f','\0','\1','\0',
                               '\0','\0','\1','\0',
                               '\0','\0','\0','\0',
                               '\0','\0'};

const char rdr_data[] = {'\0','\3','\0','\1'};

const char verHeader[] = {'\x08', 'V','e','r','s','i','o','n'}; //offset 4
const char ver9desc[] = {'\x04', 'd','e','s','c'};
const char geeSignature[] = {'\7','\0','C','O','p','M','o','v','e'};

const std::string drawingExt1 = ".rdr";
const std::string drawingExt2 = ".geerdr";
const std::string drawingExt3 = ".drawinghand";

const std::string dhc_config_name = "config.dat";
const std::string dhc_config_backup = "config.dat.backup";
// configuration file
const char  dhc_config1[]   = {'\x03','\x00','\x00','\x00'};
unsigned    dhc_delay       = 100;                                                       // dd delay between drawings in seconds
const char  dhc_config2[]   = {'\x01','\x00','\x00','\x00','\x00','\x00','\x00','\x00'};
unsigned    dhc_path_length = 15;                                                        // db path length
std::string dhc_path; //        = "c:\\DrawingHand\\";                                       // char[] Drawing Hand path
const char  dhc_config3[]   = {'\x0a','\x00','\x00','\x00',
                               '\x00','\x00','\x00','\x00','\x00','\x00','\x00','\x00',
                               '\x01','\x00','\x00','\x00','\x01','\x00','\x00','\x00'};
unsigned    dhc_num_draw    = 2;                                                         // dd number of drawings
// drawing record
const char  dhc_config4[]   = {'\x01','\x00','\x00','\x00'};
unsigned    dhc_name_length = 17;                                                        // db drawing name length
std::string dhc_name        = "dht2_temp_drawing";                                       // char[] drawing name
char        dhc_selected[]  = {'\x01','\x00','\x00','\x00','\x00','\x00','\x00','\x00'}; // selected
// Tiger.zip record
const char  dhc_tiger_zip[] = {'\x01','\x00','\x00','\x00',
                               '\x09', 'T','i','g','e','r','.','z','i','p',
                               '\x00','\x00','\x00','\x00','\x00','\x00','\x00','\x00'}; // unselected
const char  dhc_config5[]   = {'\x00','\xff','\xff','\xff','\xff','\x00',
                               '\xbc','\x9d','\x21','\x5a','\x00','\x00','\x00','\x00',
                               '\xbc','\x9d','\x21','\x5a','\x00','\x00','\x00','\x00',
                               '\x01','\x00','\x00','\x00','\x01','\x00','\x00','\x00'}; // install/configure dates?

//keys to send to screensaver
struct FIXED_KEYBD_INPUT {
    const DWORD type;
    const KEYBDINPUT ki;
    const char _align_INPUT[sizeof(INPUT) - sizeof(DWORD) - sizeof(KEYBDINPUT)];
};
const FIXED_KEYBD_INPUT EscKeySequence[] = {
                          {INPUT_KEYBOARD,
                           {0x1b, 0x01, 0, 0, 0}}               // Esc pressed
                         ,
                          {INPUT_KEYBOARD,
                           {0x1b, 0x01, KEYEVENTF_KEYUP, 0, 0}} // Esc released
/*
                         ,
                          {INPUT_KEYBOARD,
                           {0, 0x01, KEYEVENTF_SCANCODE, 0, 0}}                    // Esc pressed
                         ,
                          {INPUT_KEYBOARD,                                         //type
                           {0, 0x01, KEYEVENTF_SCANCODE or KEYEVENTF_KEYUP, 0, 0}} //ki.wVk, wScan, dwFlags, time, dwExtraInfo
// */
                         };


// uninitialized data
unsigned thumb_size;
int drawingType; // 1 rdr  2 geerdr  3 drawinghand
int geeVersion; // 1-4 geerdr, 5-9 drawinghand
int geeSubVersion;
bool silent_process, force_drawing, use_clipboard, batch, dh_interrupted;
int dh_is_running;
std::string rdr_pInputFile, rdr_pThumbnailFile, rdr_pOutputFile, drawingExt, exportExt;
std::vector<uchar> jpeg_buff, screen_buff, drawing_buffer, config_dat_buffer;
std::ofstream outfile;
cv::Mat drawing_corners[4];
cv::Scalar corners_mean;
cv::Mat image;
BITMAP bmp;
char path_buf[20480];
//char * envChar;


// ----------------------------------------------------------
void clear_all() {
    screen_buff.clear();
    jpeg_buff.clear();
    drawing_buffer.clear();
    config_dat_buffer.clear();
}

size_t path_length(std::string i) { // drive's "current directory" (i. e. "a:" without slash) will not be found
    size_t s = i.find_last_of('/');
    size_t b = i.find_last_of('\\');
    if (s == std::string::npos) s = b;
    if (b != std::string::npos) if (b > s) s = b;
    if (s != std::string::npos) s++; else s = 0;
    return s;
}

std::string ShortPath(std::string pn) {
    std::string p = pn.substr(0, path_length(pn));
    if (GetShortPathNameA(p.c_str(), (LPSTR) &path_buf, sizeof(path_buf)) > sizeof(path_buf)) { //(lpszLongPath,lpszShortPath,cchBuffer)
        std::cerr << "Path buffer is too small." << std::endl;
        clear_all();
	std::exit(-1);
    }
    return std::string(path_buf);
}

std::string trailSlash(std::string s) {
    unsigned c = s.size();
    if (c == 0) return "";
    c = s[c - 1];
    if ((c == '\\') or (c == '/')) return s;
    return s + "\\";
}


// ----------------------------------------------------------
void dh_run() {
    //envChar = getenv("windir"); if (envChar == NULL) envChar = getenv("WINDIR");
    //std::cout << ((envChar != NULL) ? ("envChar: " + std::string(envChar)) : "NULL") << std::endl;
    //std::string dh_exe = ((envChar == NULL) ? "DrawingHand.scr" : ShortPath(trailSlash(std::string(envChar)))) + "system32\\DrawingHand.scr";

    GetSystemDirectoryA((LPSTR) &path_buf, sizeof(path_buf));  //lpBuffer, uSize
    std::string dh_exe = ShortPath(trailSlash(std::string(path_buf))) + "DrawingHand.scr";
    if (!silent_process) std::cout << "Starting \"" << dh_exe << " /s\"." << std::endl;
    if (system((dh_exe + " /s").c_str()) == 1) {
        std::cerr << "System shell returned error." << std::endl;
        dh_is_running = -1;
    } else {
        dh_is_running = 0;
    }
}


// ----------------------------------------------------------
int main(int argc, char** argv) {

    // definition of command line arguments
    TCLAP::CmdLine cmd("dh-thumb", ' ', "1.41");

    TCLAP::ValueArg<std::string> cmdInputFile("i", "input-drawing",
                    "Original drawing filename.", true,
                    "", "string", cmd);

    TCLAP::ValueArg<std::string> cmdOutputFile("o", "output-drawing",
                    "Output drawing (\"name.thumb.extension\" by default).\n"
                    "Backup of DH configuration, if used, will be written to this drawing's location too.", false,
                    "", "string", cmd);

    TCLAP::ValueArg<std::string> cmdThumbnailFile("t", "thumbnail",
                    "Load drawing image from file.", false,
                    "", "string", cmd);

    TCLAP::ValueArg<bool> cmdClipboard("c", "clipboard",
                    "Import drawing image from clipboard. [0 = no]\n"
                    "If none of '-t' and '-c 1' are used, program will try to run the screensaver.", false,
                    false, "boolean", cmd);

    TCLAP::ValueArg<bool> cmdForce("f", "force",
                    "Create new drawing even if thumbnail is already present in old one. [0 = no]", false,
                    false, "boolean", cmd);

    TCLAP::ValueArg<bool> cmdSilent("s", "silent",
                    "Show only errors. [0 = no]", false,
                    false, "boolean", cmd);

    TCLAP::ValueArg<std::string> cmdDHdir("d", "dh-path",
                    "Home directory of DH screensaver. [c:\\DrawingHand\\]\n"
                    "Note that if default directory doesn't exist or you'll specify nonexisting path, current folder will be used.", false,
                    "c:\\DrawingHand\\", "string", cmd);

    TCLAP::ValueArg<bool> cmdBatch("b", "batch",
                    "Batch mode, do not backup/restore config.dat automatically, to reduce disk usage.\n"
                    "(Backup and restore manually or delete config.dat, because it will be broken!) [0 = no]", false,
                    false, "boolean", cmd);

    TCLAP::ValueArg<std::string> cmdThumbnailExport("x", "extension",
                    "Save cropped thumbnail to \"name.thumb.rgd.extension\".\n"
                    "Useful for checking thumbnail images.\n"
                    "Possible extensions are: png, bmp, tif etc.", false,
                    "", "string", cmd);

    // parse command line arguments
    try {
       	cmd.parse(argc, argv);
    } catch (std::exception &e) {
	std::cerr << e.what() << std::endl;
        std::cerr << "Error : cmd.parse() threw exception" << std::endl;
        return -1;
    }

    // command line accepted, begin console processing
    rdr_pInputFile     = cmdInputFile.getValue();
    rdr_pThumbnailFile = cmdThumbnailFile.getValue();
    rdr_pOutputFile    = cmdOutputFile.getValue();
    silent_process     = cmdSilent.getValue();
    force_drawing      = cmdForce.getValue();
    use_clipboard      = cmdClipboard.getValue();
    batch              = cmdBatch.getValue();
    dhc_path           = ShortPath(trailSlash(cmdDHdir.getValue()));
    exportExt          = cmdThumbnailExport.getValue();


// -----------------------------------------------------------------------------
// checking extension
    size_t InputExtPos = path_length(rdr_pInputFile);
    std::string drawingExt = rdr_pInputFile.substr(InputExtPos, rdr_pInputFile.size() - InputExtPos);
    //std::cout << "rdr_pInputFile name: " << drawingExt << std::endl;
    InputExtPos = drawingExt.find_last_of('.');
    if (InputExtPos == std::string::npos) {
err_unknown_extension:
        std::cerr << "\nUnknown extension of \"" << rdr_pInputFile << "\".\nValid are .rdr, .geerdr, .drawinghand" << std::endl;
        return -1;
    } else {
        drawingExt.erase(0, InputExtPos);
        InputExtPos = rdr_pInputFile.size() - drawingExt.size();
        for (std::string::size_type i = 0; i < drawingExt.length(); i++) drawingExt[i] = tolower(drawingExt[i]);
        if      (drawingExt == drawingExt1) drawingType = 1; // .rdr
        else if (drawingExt == drawingExt2) drawingType = 2; // .geerdr
        else if (drawingExt == drawingExt3) drawingType = 3; // .drawinghand
        else goto err_unknown_extension;
    }
    //std::cout << "Drawing: " << rdr_pInputFile << std::endl;
    //std::cout << "Drawing extension: " << drawingExt << std::endl;
    //std::cout << "At position: " << InputExtPos << std::endl;
    //std::cout << "Type: " << drawingType << std::endl;


// -----------------------------------------------------------------------------
// loading drawing to memory
    if (!silent_process) std::cout << "Reading the drawing: \"" << rdr_pInputFile << "\"." << std::endl;
    std::ifstream drawing (rdr_pInputFile, std::ifstream::binary);
    if (!drawing) {
drawing_error_msg:
        std::cerr << "\nError opening the drawing \"" << rdr_pInputFile << "\"." << std::endl;
        return -1;
    }
    drawing.seekg (0, drawing.end); // get length of drawing file
    int drawing_length = drawing.tellg();
    if ((drawing_length < 0) or (drawing_length > 100000000)) {   // limit to 100 Mb
        drawing.close();
        goto drawing_error_msg;
    }
    drawing.seekg (0, drawing.beg);
    drawing_buffer.resize(drawing_length); // allocate memory
    drawing.read (reinterpret_cast<char*>(drawing_buffer.data()), drawing_length); // read data as a block
    drawing.close();


// -----------------------------------------------------------------------------
// extracting output path
    std::string outPath;
    if (rdr_pOutputFile.size() == 0) outPath = rdr_pInputFile; else outPath = rdr_pOutputFile;
    outPath.erase(path_length(outPath), outPath.size());
    //std::cout << "Output path: \"" << outPath << "\"." << std::endl;


// -----------------------------------------------------------------------------
// getting drawing's image
    if (rdr_pThumbnailFile.size() != 0) {
        // loading thumbnail from file
        if (!silent_process) std::cout << "Reading thumbnail image: \"" << rdr_pThumbnailFile << "\"." << std::endl;
        image = cv::imread(rdr_pThumbnailFile, cv::IMREAD_COLOR);
        if (image.data == NULL) {
            std::cerr << "\ncv::imread : error reading image \"" << rdr_pThumbnailFile << "\"." << std::endl;
            clear_all();
            return -1;
        }
    } else if (use_clipboard) {
        // importing image from clipboard in RGB32 format
        if (!silent_process) std::cout << "Importing thumbnail image from clipboard." << std::endl;
        if (OpenClipboard(0)) {
            //std::cout << "Clipboard opened." << std::endl;
            if (IsClipboardFormatAvailable(CF_BITMAP)) {
                //if (!silent_process) std::cout << "Found image on clipboard." << std::endl;
                HBITMAP clp_hbmp = (HBITMAP) GetClipboardData(CF_BITMAP);
                if (clp_hbmp) {
                    //std::cout << "Got clipboard data." << std::endl;
                    if (GetObject(clp_hbmp, sizeof(BITMAP), &bmp) == sizeof(BITMAP)) {
                        //std::cout << "BITMAP.bmType: " << bmp.bmType << std::endl;
                        //std::cout << "BITMAP.bmWidth: " << bmp.bmWidth << std::endl;
                        //std::cout << "BITMAP.bmHeight: " << bmp.bmHeight << std::endl;
                        //std::cout << "BITMAP.bmWidthBytes: " << bmp.bmWidthBytes << std::endl;
                        //std::cout << "BITMAP.bmPlanes: " << bmp.bmPlanes << std::endl;
                        //std::cout << "BITMAP.bmBitsPixel: " << bmp.bmBitsPixel << std::endl;
                        //unsigned bpp = bmp.bmPlanes * bmp.bmBitsPixel;
                        //std::cout << "bpp: " << bpp << std::endl;

                        const unsigned rgb32_header_size = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
                        unsigned rgb32_image_size = bmp.bmWidth * bmp.bmHeight << 2;
                        jpeg_buff.resize(rgb32_header_size + rgb32_image_size);
                        BITMAPFILEHEADER* pbmph = (BITMAPFILEHEADER*) jpeg_buff.data();
                        BITMAPINFO* pbmi = (BITMAPINFO*) (jpeg_buff.data() + sizeof(BITMAPFILEHEADER));
                        pbmph->bfType = 0x4d42; //BM
                        pbmph->bfSize = rgb32_header_size + rgb32_image_size;
                        pbmph->bfReserved1 = 0;
                        pbmph->bfReserved1 = 0;
                        pbmph->bfOffBits = rgb32_header_size;
                        pbmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                        pbmi->bmiHeader.biWidth = bmp.bmWidth;
                        pbmi->bmiHeader.biHeight = bmp.bmHeight;
                        pbmi->bmiHeader.biPlanes = 1;
                        pbmi->bmiHeader.biBitCount = 32;
                        pbmi->bmiHeader.biCompression = BI_RGB;
                        pbmi->bmiHeader.biSizeImage = rgb32_image_size;
                        pbmi->bmiHeader.biXPelsPerMeter = 0;
                        pbmi->bmiHeader.biYPelsPerMeter = 0;
                        pbmi->bmiHeader.biClrUsed = 0;
                        pbmi->bmiHeader.biClrImportant = 0;
                        HDC desktop_hdc = GetDC(HWND_DESKTOP);
                        if (desktop_hdc) {
                            //std::cout << "desktop_hdc taken." << std::endl;
                            //                                                                  V pbmph does not work here (writes 702 zeroes between header and image), why?
                            int rows_copied = GetDIBits(desktop_hdc, clp_hbmp, 0, bmp.bmHeight, jpeg_buff.data() + rgb32_header_size, pbmi, DIB_RGB_COLORS);
                            ReleaseDC(HWND_DESKTOP, desktop_hdc); //if (ReleaseDC(HWND_DESKTOP, desktop_hdc) == 1) std::cout << "desktop_hdc released." << std::endl;
                            //std::cout << rows_copied << " rows copied from clipboard." << std::endl;
                            if (rows_copied != 0) {
                                //outfile = std::ofstream("rdr.clp.bmp", std::ofstream::binary); if (!outfile) { std::cerr << "\nError writing \"rdr.clp.bmp\"." << std::endl; clear_all(); return -1; }; outfile.write (reinterpret_cast<const char*>(jpeg_buff.data()), jpeg_buff.size()); outfile.close();
                                image = cv::imdecode(jpeg_buff, cv::IMREAD_COLOR);
                            }
                        }
                    }
                }
                // Do not close the handle of GetClipboardData, it should be operated by system.
            }
            CloseClipboard();
        }
        if (image.data == NULL) {
            std::cerr << "\nError importing image from clipboard.\n"
                           "Try to copy a screenshot again or use \"-t\" switch." << std::endl;
            clear_all();
            return -1;
        }
    } else {
        // capturing the drawing
        std::string dh_temp_drawing = dhc_name + drawingExt;
        // ---------------------------------------------------------------------
        // backuping configuration file
        std::string dhc_config_dat = dhc_path + dhc_config_name;
        int old_config_length = 0;
        if (!batch) {
            //if (!silent_process) std::cout << "Backuping " << dhc_config_name << "." << std::endl;
            if (!silent_process) std::cout << "Reading \"" << dhc_config_dat << "\"." << std::endl;
            std::ifstream old_config_dat (dhc_config_dat, std::ifstream::binary);
            if (!old_config_dat) {
old_config_error_msg:
                std::cerr << "\nError reading \"" << dhc_config_dat << "\".\n"
                             "If the DH screensaver is installed, please configure it at least once or specify its path with '-d' option.\n"
                             "If it's not installed, please use '-t' or '-c' options." << std::endl;
                clear_all();
                return -1;
            }
            old_config_dat.seekg (0, old_config_dat.end); // get length
            old_config_length = old_config_dat.tellg();
            if (old_config_length < 0) {
                old_config_dat.close();
                goto old_config_error_msg;
            }
            old_config_dat.seekg (0, old_config_dat.beg);
            config_dat_buffer.resize(old_config_length); // allocate memory
            old_config_dat.read (reinterpret_cast<char*>(config_dat_buffer.data()), old_config_length);
            old_config_dat.close();

            if (!silent_process) std::cout << "Writing \"" << outPath << dhc_config_backup << "\"." << std::endl;
            outfile = std::ofstream(outPath + dhc_config_backup, std::ofstream::binary);
            if (!outfile) {
                std::cerr << "\nError writing " << dhc_config_backup << "." << std::endl;
                clear_all();
                return -1;
            }
            outfile.write (reinterpret_cast<const char*>(config_dat_buffer.data()), old_config_length);
            outfile.close();
        }
        // ---------------------------------------------------------------------
        // generating temporary configuration
        if (!silent_process) std::cout << "Writing temporary " << dhc_config_name << "." << std::endl;
        //outfile = std::ofstream(dhc_config_dat + "+", std::ofstream::binary);
        outfile = std::ofstream(dhc_config_dat, std::ofstream::binary);
        if (!outfile) {
            std::cerr << "\nError writing temporary \"" << dhc_config_dat << "\"." << std::endl;
            clear_all();
            return -1;
        }
        outfile.write (&dhc_config1[0], sizeof(dhc_config1));
        outfile.write (reinterpret_cast<const char*>(&dhc_delay), 4);
        outfile.write (&dhc_config2[0], sizeof(dhc_config2));
        dhc_path_length = dhc_path.size();
        outfile.write (reinterpret_cast<const char*>(&dhc_path_length), 1);
        outfile.write (reinterpret_cast<const char*>(dhc_path.c_str()), dhc_path_length);
        outfile.write (&dhc_config3[0], sizeof(dhc_config3));
        outfile.write (reinterpret_cast<const char*>(&dhc_num_draw), 4);
        outfile.write (&dhc_config4[0], sizeof(dhc_config4));
        dhc_name_length = dh_temp_drawing.size(); //dhc_name.size();
        outfile.write (reinterpret_cast<const char*>(&dhc_name_length), 1);
        outfile.write (reinterpret_cast<const char*>(dh_temp_drawing.c_str()), dhc_name_length);
        outfile.write (&dhc_selected[0], sizeof(dhc_selected));
        outfile.write (&dhc_tiger_zip[0], sizeof(dhc_tiger_zip));
        outfile.write (&dhc_config5[0], sizeof(dhc_config5));
        outfile.close();
        // ---------------------------------------------------------------------
        // writing temporary drawing
        if (!silent_process) std::cout << "Copying temporary drawing: \"" << dhc_path << dh_temp_drawing << "\"." << std::endl;
        outfile = std::ofstream(dhc_path + dh_temp_drawing, std::ofstream::binary);
        if (!outfile) {
            std::cerr << "\nError writing \"" << dhc_path + dh_temp_drawing << "\"." << std::endl;
            clear_all();
            return -1;
        }
        outfile.write (reinterpret_cast<const char*>(drawing_buffer.data()), drawing_length);
        outfile.close();
        // ---------------------------------------------------------------------
        //if (!silent_process) std::cout << "Running a screensaver..." << std::endl;
        dh_is_running = 1;
        dh_interrupted = false;
        long long dh_wait_end = cv::getTickCount() + cv::getTickFrequency() * dh_wait;
        std::thread(dh_run).detach();
        HWND dh_hwnd;
        while ((dh_is_running != -1) && ((dh_hwnd = FindWindowA("WindowsScreenSaverClass", NULL)) == NULL) && (cv::getTickCount() <= dh_wait_end)) {
            Sleep(300); //milliseconds
        }
        if (dh_hwnd == NULL) {
            //std::cerr << "Tired to wait a screensaver more than " << dh_wait << " seconds." << std::endl;
            if (dh_is_running == -1) {
                std::cerr << "Error running the screensaver." << std::endl;
            } else if (dh_is_running == 0) {
                std::cerr << "Screensaver seems to ran, but exited." << std::endl;
            } else if (dh_is_running == 1) {
                std::cerr << "Screensaver seems to be running, wrong window class?" << std::endl;
            }
            dh_interrupted = true;
            goto temp_cleanup;
        } else {
            //if (!silent_process) std::cout << "Detected a screensaver after " << ((cv::getTickCount() - dh_wait_end) / cv::getTickFrequency() + dh_wait) << " seconds, waiting for it to finish." << std::endl;
            //if (!silent_process) std::cout << "Getting screenshot." << std::endl;
            dh_wait_end = cv::getTickCount() + cv::getTickFrequency() * dh_wait;
            const unsigned rgb32_header_size = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
            unsigned sw = GetSystemMetrics(SM_CXSCREEN);
            unsigned sh = GetSystemMetrics(SM_CYSCREEN);
            unsigned rgb32_image_size = sw * sh << 2;
            unsigned bmp_buffer_size = rgb32_header_size + rgb32_image_size;
            // unsigned scr_num = 0;
            screen_buff.clear();
            while ((FindWindowA("WindowsScreenSaverClass", NULL) != NULL) &&
                   ((screen_buff.size() == 0) ||
                    (screen_buff.size() != jpeg_buff.size()) ||
                    (memcmp(screen_buff.data(), jpeg_buff.data(), jpeg_buff.size()) != 0))) {
                HDC screenDC = GetDC(HWND_DESKTOP);
                if (screenDC) {
                    HBITMAP bm = CreateCompatibleBitmap(screenDC, sw, sh);
                    if (bm) {
                        HDC compatDC = CreateCompatibleDC(screenDC);
                        if (compatDC) {
                            HGDIOBJ previous_object = SelectObject(compatDC, bm);
                            //bool bbr =
                            BitBlt(compatDC, 0, 0, sw, sh, screenDC, 0, 0, SRCCOPY); //dest,dx,dy, w,h, src,sx,sy, operation
                            screen_buff = jpeg_buff;
                            jpeg_buff.clear();
                            jpeg_buff.resize(bmp_buffer_size);
                            BITMAPFILEHEADER* pbmph = (BITMAPFILEHEADER*) jpeg_buff.data();
                            BITMAPINFO* pbmi = (BITMAPINFO*) (jpeg_buff.data() + sizeof(BITMAPFILEHEADER));
                            pbmph->bfType = 0x4d42; //BM
                            pbmph->bfSize = bmp_buffer_size;
                            pbmph->bfReserved1 = 0;
                            pbmph->bfReserved1 = 0;
                            pbmph->bfOffBits = rgb32_header_size;
                            pbmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                            pbmi->bmiHeader.biWidth = sw;
                            pbmi->bmiHeader.biHeight = sh;
                            pbmi->bmiHeader.biPlanes = 1;
                            pbmi->bmiHeader.biBitCount = 32;
                            pbmi->bmiHeader.biCompression = BI_RGB;
                            pbmi->bmiHeader.biSizeImage = rgb32_image_size;
                            pbmi->bmiHeader.biXPelsPerMeter = 0;
                            pbmi->bmiHeader.biYPelsPerMeter = 0;
                            pbmi->bmiHeader.biClrUsed = 0;
                            pbmi->bmiHeader.biClrImportant = 0;
                            //int rows_copied =
                            GetDIBits(compatDC, bm, 0, sh, jpeg_buff.data() + rgb32_header_size, pbmi, DIB_RGB_COLORS);
                            //if (rows_copied != 0) { outfile = std::ofstream("scr" + std::to_string(scr_num) + ".bmp", std::ofstream::binary); if (!outfile) { std::cerr << "\nError writing \"rdr.clp.bmp\"." << std::endl; clear_all(); return -1; }; outfile.write (reinterpret_cast<const char*>(jpeg_buff.data()), jpeg_buff.size()); outfile.close(); scr_num++; };
                            SelectObject(compatDC, previous_object);
                            DeleteDC(compatDC);
                        }
                        DeleteObject(bm);
                    }
                    ReleaseDC(HWND_DESKTOP, screenDC);
                }
                Sleep(dhc_delay * 100); // dhc_delay / 10 * 1000   (n seconds, 1/10 of tdelay between drawings)
            }
            if (FindWindowA("WindowsScreenSaverClass", NULL) == NULL) {
                screen_buff.clear();
                jpeg_buff.clear();
                dh_interrupted = true;
                goto temp_cleanup;
            }
            screen_buff.clear();
            image = cv::imdecode(jpeg_buff, cv::IMREAD_COLOR);
            jpeg_buff.clear();
            if (image.data == NULL) {
                std::cerr << "\nError getting the screenshot." << std::endl;
                dh_interrupted = true;
                goto temp_cleanup;
            }
            if (!silent_process) std::cout << "Screenshot taken in " <<
                                           ((cv::getTickCount() - dh_wait_end) / cv::getTickFrequency() + dh_wait)
                                           << " seconds." << std::endl;
            //stopping the screensaver by simulating Esc presses
            dh_wait_end = cv::getTickCount() + cv::getTickFrequency() * dh_wait;
            /*
            EscKeySequence[0].type = INPUT_KEYBOARD;           //INPUT.DWORD
            EscKeySequence[0].ki.wVk = 0;                      //INPUT.KEYBDINPUT.WORD, 0 when sending keyboard scan code
            EscKeySequence[0].ki.wScan = 1;                    //INPUT.KEYBDINPUT.WORD, Escape scan code, https://msdn.microsoft.com/en-us/library/aa299374(v=vs.60).aspx
            EscKeySequence[0].ki.dwFlags = KEYEVENTF_SCANCODE; //INPUT.KEYBDINPUT.DWORD
            EscKeySequence[0].ki.time = 0;                     //INPUT.KEYBDINPUT.DWORD
            EscKeySequence[0].ki.dwExtraInfo = 0;              //INPUT.KEYBDINPUT.ULONG_PTR
            EscKeySequence[1].type = INPUT_KEYBOARD;
            EscKeySequence[1].ki.wVk = 0;
            EscKeySequence[1].ki.wScan = 1;
            EscKeySequence[1].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP; //INPUT.KEYBDINPUT.DWORD
            EscKeySequence[1].ki.time = 0;
            EscKeySequence[1].ki.dwExtraInfo = 0;
            // */
            unsigned nKeys = (sizeof(EscKeySequence) / sizeof(INPUT));
            //std::cout << "INPUT: " << sizeof(INPUT) << ", FIXED_KEYBD_INPUT: " << sizeof(FIXED_KEYBD_INPUT) << std::endl;
            /*
            std::cout << "nKeys: " << nKeys << std::endl;
            for (unsigned i = 0; i < nKeys; i++) {
                std::cout << "# " << i << std::endl;
                std::cout << "INPUT.type "           << EscKeySequence[i].type           << std::endl;
                std::cout << "INPUT.ki.wVk "         << EscKeySequence[i].ki.wVk        << std::endl;
                std::cout << "INPUT.ki.wScan "       << EscKeySequence[i].ki.wScan       << std::endl;
                std::cout << "INPUT.ki.dwFlags "     << EscKeySequence[i].ki.dwFlags     << std::endl;
                std::cout << "INPUT.ki.time "        << EscKeySequence[i].ki.time        << std::endl;
                std::cout << "INPUT.ki.dwExtraInfo " << EscKeySequence[i].ki.dwExtraInfo << std::endl;
            }
            // */
            while ((dh_is_running == 1) && (cv::getTickCount() <= dh_wait_end)) {
                if (SendInput(nKeys, (LPINPUT) &EscKeySequence, sizeof(INPUT)) != nKeys) { //UINT nInputs, LPINPUT pInput, int cbSize
                    std::cerr << "Can not send Esc keypress." << std::endl;
                    dh_interrupted = true;
                    goto temp_cleanup;
                }
                Sleep(300); // send Esc every 0.3 seconds
            }
            if (dh_is_running == 1) {
                std::cerr << "Screensaver seems to be still running after " << dh_wait << " seconds of attempts to stop it..." << std::endl;
                dh_interrupted = true;
                //goto temp_cleanup;
            }
        }
        //if (!silent_process) std::cout << "Screensaver thread exited." << std::endl;
temp_cleanup:
        // -----------------------------------------------------------------------------
        // deleting temporary drawing
        if (!silent_process) std::cout << "Removing temporary drawing: \"" << dhc_path << dh_temp_drawing << "\"." << std::endl;
        remove((dhc_path + dh_temp_drawing).c_str());
        // -----------------------------------------------------------------------------
        // restoring original configuration
        if (!batch) {
            if (!silent_process) std::cout << "Restoring " << dhc_config_name << "." << std::endl;
            outfile = std::ofstream(dhc_config_dat, std::ofstream::binary);
            if (!outfile) {
                std::cerr << "\nError restoring \"" << dhc_config_dat << "\"." << std::endl;
                clear_all();
                return -1;
            }
            outfile.write (reinterpret_cast<const char*>(config_dat_buffer.data()), old_config_length);
            outfile.close();
        }
        config_dat_buffer.clear();
        if (dh_interrupted) {
            std::cerr << "\nThe process was interrupted, screenshot is not taken." << std::endl;
            clear_all();
            return -1;
        }
    }


// -----------------------------------------------------------------------------
// processing drawing

// drawing_buffer  = drawing data
// drawing_length  = drawing size
// rdr_pInputFile  = drawing name
// drawingExt      =             .rdr/.geerdr/.drawinghand
// InputExtPos     =             ^
// drawingType     =             1    2       3
// outPath         = output path
// rdr_pOutputFile = output file

//cv::imwrite("screen.png", image);

    if (rdr_pOutputFile == "")
        rdr_pOutputFile = rdr_pInputFile.substr(0, InputExtPos) + ".thumb" + drawingExt;

    bool drawing_written = false;

    if (!silent_process) std::cout << "Cropping image." << std::endl;
    unsigned iw = image.size().width;
    unsigned ih = image.size().height;
    unsigned ct = 0;
    unsigned cb = ih;
    unsigned cl = 0;
    unsigned cr = iw;
    float edge = 0;

    // cv::Vec3f image.at(ih-1,iw-1) crashes, therefore using cv::Mat
    cv::Mat(image, cv::Rect(     0,      0, 1, 1)).convertTo(drawing_corners[0], CV_32F); //cv::Rect(x,y,width,height)
    cv::Mat(image, cv::Rect(iw - 1,      0, 1, 1)).convertTo(drawing_corners[1], CV_32F);
    cv::Mat(image, cv::Rect(     0, ih - 1, 1, 1)).convertTo(drawing_corners[2], CV_32F);
    cv::Mat(image, cv::Rect(iw - 1, ih - 1, 1, 1)).convertTo(drawing_corners[3], CV_32F);
    //std::cout << "corners: " << drawing_corners[0] << drawing_corners[1] << drawing_corners[2] << drawing_corners[3] << std::endl;
    corners_mean = cv::mean((drawing_corners[0] +
                             drawing_corners[1] +
                             drawing_corners[2] +
                             drawing_corners[3]) / 4);
    //std::cout << "corners' mean: " << corners_mean << std::endl;
    float corners_average = (corners_mean[0] + corners_mean[1] + corners_mean[2]) / 3;
    //std::cout << "corners_average: " << corners_average << std::endl;

    for (unsigned r = 0; r < ih; r++) {
        cv::Mat row = cv::Mat(image, cv::Rect(0, r, iw, 1)); //cv::Rect(x,y,width,height)
        cv::Scalar m = cv::mean(row);
        float e = abs((m[0] + m[1] + m[2]) / 3 - corners_average);
        if (e != 0) {
            ct = r;
            edge += e;
            break;
        }
    }
    for (int r = ih - 1; r >= 0; r--) {
        cv::Mat row = cv::Mat(image, cv::Rect(0, r, iw, 1));
        cv::Scalar m = cv::mean(row);
        float e = abs((m[0] + m[1] + m[2]) / 3 - corners_average);
        if (e != 0) {
            cb = static_cast<unsigned>(r) + 1;
            edge += e;
            break;
        }
    }
    for (unsigned c = 0; c < iw; c++) {
        cv::Mat column = cv::Mat(image, cv::Rect(c, 0, 1, ih));
        cv::Scalar m = cv::mean(column);
        float e = abs((m[0] + m[1] + m[2]) / 3 - corners_average);
        if (e != 0) {
            cl = c;
            edge += e;
            break;
        }
    }
    for (int c = iw - 1; c >= 0; c--) {
        cv::Mat column = cv::Mat(image, cv::Rect(c, 0, 1, ih));
        cv::Scalar m = cv::mean(column);
        float e = abs((m[0] + m[1] + m[2]) / 3 - corners_average);
        if (e != 0) {
            cr = static_cast<unsigned>(c) + 1;
            edge += e;
            break;
        }
    }
    //std::cout << "Width: " << cl << "><" << iw - cr << ", height: " << ct << "><" << ih - cb << std::endl;
    cl = std::min(cl, iw - cr);
    ct = std::min(ct, ih - cb);
    //std::cout << "edge: " << edge << std::endl;
    if (edge < 12.0) { // 3.0 * [left, right, top, bottom]
        if (!silent_process) std::cout << "Edges are not solid (" << edge / 4 << " < 3), some borders left." << std::endl;
        cl = static_cast<unsigned>(std::max(0, static_cast<int>(cl) - 17));
        ct = static_cast<unsigned>(std::max(0, static_cast<int>(ct) - 17));
    }
    //if (!silent_process) std::cout << "Horisontal crop: " << cl << ", vertical crop: " << ct << " (" << iw - cl * 2 << "x" << ih - ct * 2 << ")." << std::endl;
    image = cv::Mat(image, cv::Rect(cl, ct, iw - cl * 2, ih - ct * 2)); //cv::Rect(x,y,width,height)

    if (exportExt.size() != 0) {
        std::string ThumbPreview = rdr_pOutputFile;
        if (exportExt[0] != '.') ThumbPreview += ".";
        ThumbPreview += exportExt;
        if (!silent_process) std::cout << "Writing cropped image: \"" << ThumbPreview << "\"." << std::endl;
        if (!cv::imwrite(ThumbPreview, image)) std::cerr << "\nError saving cropped image \"" << ThumbPreview << "\". Please check that it's not write-protected." << std::endl;
    }


    cv::Size imageSize = image.size();
    double Xds = 119.0 / static_cast<double>(imageSize.width);
    double Yds = 85.0 / static_cast<double>(imageSize.height);
    if (Xds <= Yds) {
	imageSize.width  = static_cast<int>(static_cast<double>(imageSize.width)  * Xds);
	imageSize.height = static_cast<int>(static_cast<double>(imageSize.height) * Xds);
    } else {
	imageSize.width  = static_cast<int>(static_cast<double>(imageSize.width)  * Yds);
	imageSize.height = static_cast<int>(static_cast<double>(imageSize.height) * Yds);
    }
    cv::resize(image, image, imageSize * 32, 0, 0, cv::INTER_LANCZOS4); //cv::INTER_LANCZOS4 cv::INTER_CUBIC cv::INTER_AREA cv::INTER_LINEAR
    cv::resize(image, image, imageSize,      0, 0, cv::INTER_AREA);
    if (!silent_process) std::cout << "Scaling to 119x85 pixels (" << imageSize.width << "x" << imageSize.height << ")." << std::endl;


    cv::imencode(".jpg", image, jpeg_buff, OpenCV_JPEG_options);
    thumb_size = jpeg_buff.size();


// -----------------------------------------------------------------------------
    unsigned data_pos = 0;
    unsigned w1 = 0;
    unsigned h1 = 0;

if (drawingType == 1) {
    //.rdr
// NN NN ................................................. NN NN WWWW HHHH NN NN NN NN NN NN NN NN 00 03 00 01 palette 04 ...
// 2f 00 01 00 00 00 01 00 00 00 00 00 00 00 LLLLLLLL jpeg NN NN WWWW HHHH NN NN NN NN NN NN NN NN 00 03 00 01 palette 04 ...

    unsigned tl;
    if (memcmp(drawing_buffer.data(), &rdr_new_header, sizeof(rdr_new_header)) == 0) {
        tl = *((unsigned*) (drawing_buffer.data() + sizeof(rdr_new_header)));
        // std::cout << "thumbnail length = " << tl << std::endl;
        data_pos = data_pos + sizeof(rdr_new_header) + 4 + tl + 14;
        if (data_pos + sizeof(rdr_data) + 4 < static_cast<unsigned>(drawing_length)) {
            if (memcmp(drawing_buffer.data() + data_pos, &rdr_data, sizeof(rdr_data)) == 0) {
                if (drawing_buffer[data_pos + sizeof(rdr_data) + 3] == '\4') {
                    if (!force_drawing || !silent_process)
                        std::cout << "The drawing \"" << rdr_pInputFile << "\" already contains thumbnail (" << tl << " bytes)." << std::endl;
                    if (!force_drawing) {
                        std::cout << "Skipped." << std::endl;
                        goto finish_drawing1;
                    }
                }
            }
        }
    } else
        tl = 0;

    // tl = search limit
    tl = std::min(tl + 1000000, static_cast<unsigned>(drawing_length) - sizeof(rdr_data));
    while (data_pos <= tl) {
        data_pos = std::find(drawing_buffer.data() + data_pos, drawing_buffer.data() + drawing_length - sizeof(rdr_data) + 1, rdr_data[0]) - drawing_buffer.data();
        if (data_pos < (drawing_length - sizeof(rdr_data) + 1)) {
            if (memcmp(drawing_buffer.data() + data_pos, &rdr_data, sizeof(rdr_data)) == 0) {
                if (drawing_buffer[data_pos + sizeof(rdr_data) + 3] == '\4') {
                    w1 = *((short*) (drawing_buffer.data() + data_pos - 12));
                    h1 = *((short*) (drawing_buffer.data() + data_pos - 10));
                    //if (!silent_process) std::cout << "Canvas size: " << w1 << "x" << h1 << " pixels." << std::endl;
                    if ((h1 == 0) or (w1 == 0)) {
                        std::cout << "\nWarning: zero width or height found." << std::endl;
                        if (!silent_process) {
                            std::cout << "Is it really a drawing?\n" << std::endl;
                        } else {
                            std::cout << "Is \"" << rdr_pInputFile << "\" really a drawing?" << std::endl;
                        }
                    }
                    data_pos = data_pos - 14;

                    if (!silent_process) std::cout << "Writing new RDR drawing: \"" << rdr_pOutputFile << "\"." << std::endl;
                    outfile = std::ofstream(rdr_pOutputFile, std::ofstream::binary);
                    if (!outfile) {
                        std::cerr << "\nError writing the drawing \"" << rdr_pOutputFile << "\"." << std::endl;
                        clear_all();
                        return -1;
                    }
                    outfile.write (&rdr_new_header[0], sizeof(rdr_new_header));
                    outfile.write (reinterpret_cast<const char*>(&thumb_size), 4);
                    outfile.write (reinterpret_cast<const char*>(jpeg_buff.data()), thumb_size);
                    outfile.write (reinterpret_cast<const char*>(drawing_buffer.data() + data_pos), drawing_length - data_pos);
                    outfile.close();
                    drawing_written = true;
                    goto finish_drawing1;
                }
            } else
                data_pos++;
            //if (data_pos > prev_pos) { std::cout << "data_pos: " << data_pos << std::endl; prev_pos = data_pos + 1000; }
        } else
            break; // pattern was not found
    }

    std::cout << "\nDrawing data is not found in \"" << rdr_pInputFile << "\".\nCan not make valid drawing file." << std::endl;
    clear_all();
    return -1;

finish_drawing1:
    clear_all(); //nop
    //goto finish_drawing;

} else if( (drawingType == 3) || (drawingType == 2) ){
    //.drawinghand, .geerdr
// 0  1  2  3  4   56789012  13 14 15 16 17 18 19 20 21  2345  22/26         0        4        8  9  10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29
// VV 00 00 00 08 "Version9" NN 00 00 00 01 00 00 00 04 "desc" LLLLLLLL bmp  WWWWWWWW HHHHHHHH ff ff ff 00 02 00 00 00 ff ff ff ff 00 ac 5e 00 00 ff ff 00 00 07 00 COpMove ...
// VV 00 00 00 08 "Version8" NN 00 00 00 01 00 00 00 00        LLLLLLLL jpeg WWWWWWWW HHHHHHHH ff ff ff 00 02 00 00 00 ff ff ff ff 00 ac 5e 00 00 ff ff 00 00 07 00 COpMove ...
// VV 00 00 00 08 "Version7"                                                 WWWWWWWW HHHHHHHH ff ff ff 00 02 00 00 00 ff ff ff ff 00 ac 5e 00 00 ff ff 00 00 07 00 COpMove ...
// 01 02? 03? 04 - geerdr, 05 06 - drawinghand
//                           NN 00 00 00                                     WWWWWWWW HHHHHHHH ff ff ff 00 02 00 00 00 ff ff ff ff 00 ac 5e 00 00 ff ff 00 00 07 00 COpMove ...

    int geeVersionL = 0;
    if (memcmp(drawing_buffer.data() + 4, &verHeader, sizeof(verHeader)) == 0) {
        //std::cout << "found VersionX header" << std::endl;
        geeVersion = drawing_buffer[4 + sizeof(verHeader)] - '0'; //ver=8,9,?
        //if (!silent_process) std::cout << "Geerdr version: " << geeVersion << std::endl;
        if (drawing_buffer[0] != geeVersion) {
            std::cout << "Header version does not match 'Version'. Stop." << std::endl;
            clear_all();
            return -1;
        }
        if (geeVersion == 7) {
            // Version 7 doesn't seem to not support thumbnails, changing to version 8, subversion 7.
            geeVersion = 8;
            geeSubVersion = 7;
            data_pos = 13;
            goto headerless_geerdr;
        }
        geeSubVersion = *((int*)(drawing_buffer.data() + 13));
        //if (!silent_process) std::cout << "Geerdr subversion: " << geeSubVersion << std::endl;
        data_pos = 22;
        if (memcmp(drawing_buffer.data() + 21, &ver9desc, sizeof(ver9desc)) == 0) {
            if (geeVersion != 9) {
                std::cout << "'desc' found, but Version is not 9. Stop." << std::endl;
                clear_all();
                return -1;
            }
            data_pos = data_pos + 4;
        }
        unsigned tl = *((unsigned*)(drawing_buffer.data() + data_pos));
        //std::cout << "Thumbnail length: " << tl << " bytes." << std::endl;

        // checking thumbnail format
        short thumbFmt = *((short*)(drawing_buffer.data() + data_pos + 4));
        if ((thumbFmt != 0x4d42) && (thumbFmt != -9985)) { // "BM" - bmp thumbnail, 0xd8ff - jpeg thumbnail
            std::cerr << "Warning: unknown thumbnail format (" << thumbFmt << ").\nOnly BMP and JPEG are supported." << std::endl;
            //clear_all();
            //return -1;
        }

        data_pos = data_pos + 4 + tl;
        w1 = *((unsigned*) (drawing_buffer.data() + data_pos));
        h1 = *((unsigned*) (drawing_buffer.data() + data_pos + 4));
        //if (!silent_process) std::cout << "Canvas size: " << w1 << "x" << h1 << " pixels." << std::endl;
        if ((h1 == 0) or (w1 == 0)) {
            std::cout << "\nWarning: zero width or height found." << std::endl;
            if (!silent_process) {
                std::cout << "Is it really a drawing?\n" << std::endl;
            } else {
                std::cout << "Is \"" << rdr_pInputFile << "\" really a drawing?" << std::endl;
            }
        }
        if (memcmp(drawing_buffer.data() + data_pos + 29, &geeSignature, sizeof(geeSignature)) == 0) {
            //std::cout << "geeSignature found." << std::endl;
            if (!force_drawing || !silent_process)
                std::cout << "The drawing \"" << rdr_pInputFile << "\" already contains thumbnail (" << tl << " bytes)." << std::endl;
            if (!force_drawing) {
                std::cout << "Skipped." << std::endl;
                goto finish_drawing2;
            }
        } else
            goto no_gee_sig;
    } else { // no "VersionX" header - versions 1...6, changing to version 8, subversion N
        geeVersion = 8;
        geeSubVersion = *((int*) drawing_buffer.data());
        data_pos = 4;
headerless_geerdr:
        //if (!silent_process) std::cout << "No thumbnail." << std::endl;
        w1 = *((int*) (drawing_buffer.data() + data_pos));
        h1 = *((int*) (drawing_buffer.data() + data_pos + 4));
        //if (!silent_process) std::cout << "Canvas size: " << w1 << "x" << h1 << " pixels." << std::endl;
        if ((h1 == 0) or (w1 == 0)) {
            std::cout << "\nWarning: zero width or height found." << std::endl;
            if (!silent_process) {
                std::cout << "Is it really a drawing?\n" << std::endl;
            } else {
                std::cout << "Is \"" << rdr_pInputFile << "\" really a drawing?" << std::endl;
            }
        }
        if (memcmp(drawing_buffer.data() + data_pos + 29, &geeSignature, sizeof(geeSignature)) != 0) {
no_gee_sig:
            std::cout << "\nDrawing data is not found in \"" << rdr_pInputFile << "\".\nCan not make valid drawing file." << std::endl;
            clear_all();
            return -1;
        } //else {
         //   std::cout << "geeSignature found." << std::endl;
        //}
    }

    if ( (geeVersion != 8) and (geeVersion != 9) ) {
        std::cout << "\n'Version' is not 8 or 9. Stop." << std::endl;
        clear_all();
        return -1;
    }

    if (!silent_process) std::cout << "Writing new drawing: \"" << rdr_pOutputFile << "\"." << std::endl;
    outfile = std::ofstream(rdr_pOutputFile, std::ofstream::binary);
    if (!outfile) {
        std::cerr << "\nError writing the drawing \"" << rdr_pOutputFile << "\"." << std::endl;
        clear_all();
        return -1;
    }
    outfile.write (reinterpret_cast<const char*>(&geeVersion), 4);
    outfile.write (&verHeader[0], sizeof(verHeader));
    geeVersionL = geeVersion + '0';
    outfile.write (reinterpret_cast<const char*>(&geeVersionL), 1);
    outfile.write (reinterpret_cast<const char*>(&geeSubVersion), 4);
    geeVersionL = 1;
    outfile.write (reinterpret_cast<const char*>(&geeVersionL), 4);
    if (geeVersion == 8) {
        geeVersionL = 0;
        outfile.write (reinterpret_cast<const char*>(&geeVersionL), 1);
    } else if (geeVersion == 9) {
        outfile.write (&ver9desc[0], sizeof(ver9desc));
    }
    outfile.write (reinterpret_cast<const char*>(&thumb_size), 4);
    outfile.write (reinterpret_cast<const char*>(jpeg_buff.data()), thumb_size);
    outfile.write (reinterpret_cast<const char*>(drawing_buffer.data() + data_pos), drawing_length - data_pos);
    outfile.close();
    drawing_written = true;

finish_drawing2:
    clear_all(); //nop
}


// - finished ------------------------------------------------------------------
    if ((iw < w1) || (ih < h1)) {
        std::cout << "\nImage dimensions (" << iw << "x" << ih << ") are smaller than drawing canvas (" << w1 << "x" << h1 << ")."
                     "\nThe thumbnail in \"" << rdr_pOutputFile << "\" " << (drawing_written ? "is" : "would be") << " cropped." << std::endl;
    }
    clear_all();
    return 0;
}
