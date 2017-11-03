/*
 * rdr.cpp
 */


#include <opencv2/opencv.hpp>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <cmath>
#include <thread>
#include "tclap/CmdLine.h"


// ----------------------------------------------------------
// initialized data
const std::vector<int> OpenCV_JPEG_options = {CV_IMWRITE_JPEG_QUALITY, 100,
                                        CV_IMWRITE_JPEG_PROGRESSIVE, 0,
                                        CV_IMWRITE_JPEG_OPTIMIZE, 1,
                                        CV_IMWRITE_JPEG_RST_INTERVAL, 0,
                                        CV_IMWRITE_JPEG_LUMA_QUALITY, 100,
                                        CV_IMWRITE_JPEG_CHROMA_QUALITY, 100}; //cv::IMWRITE_JPEG_CHROMA_QUALITY

const char rdr_new_header[] = {'\x2f','\0','\1','\0',
                               '\0','\0','\1','\0',
                               '\0','\0','\0','\0',
                               '\0','\0'};

const char rdr_data[] = {'\0','\3','\0','\1'};

const std::string drawingExt1 = ".rdr";
const std::string drawingExt2 = ".geerdr";
const std::string drawingExt3 = ".drawinghand";


// uninitialized data
unsigned thumb_size;
int drawingType; // 1 rdr  2 geerdr  3 drawinghand
bool silent_process, force_drawing;
std::string rdr_pInputFile, rdr_pThumbnailFile, rdr_pOutputFile, drawingExt;
std::vector<uchar> jpeg_buff;
std::ofstream outfile;
char * drawing_buffer;


// ----------------------------------------------------------
int main(int argc, char** argv) {

    // definition of command line arguments
    TCLAP::CmdLine cmd("dh-thumb", ' ', "1.0");

    TCLAP::ValueArg<std::string> cmdInputFile("i", "input-drawing",
                    "Original drawing filename.", true,
                    "", "string", cmd);

    TCLAP::ValueArg<std::string> cmdThumbnailFile("t", "thumbnail",
                    "Image with rendered drawing.", true,
                    "", "string", cmd);

    TCLAP::ValueArg<std::string> cmdOutputFile("o", "output-drawing",
                    "Output drawing (\"name.thumb.ext\" by default).", false,
                    "", "string", cmd);

    TCLAP::ValueArg<bool> cmdForce("f", "force",
                    "Create new drawing even if thumbnail is already present in old one. [0 = no]", false,
                    false, "boolean", cmd);

    TCLAP::ValueArg<bool> cmdSilent("s", "silent",
                    "Show only errors. [0 = no]", false,
                    false, "boolean", cmd);

    // parse command line arguments
    try {
       	cmd.parse(argc, argv);
    } catch (std::exception &e) {
	std::cerr << e.what() << std::endl;
        std::cerr << "Error : cmd.parse() threw exception" << std::endl;
	std::exit(-1);
    }

    // command line accepted, begin console processing
    rdr_pInputFile     = cmdInputFile.getValue();
    rdr_pThumbnailFile = cmdThumbnailFile.getValue();
    rdr_pOutputFile    = cmdOutputFile.getValue();
    silent_process     = cmdSilent.getValue();
    force_drawing      = cmdForce.getValue();


    // load image file
    if (!silent_process) std::cout << "Reading thumbnail image: \"" << rdr_pThumbnailFile << "\"." << std::endl;
    cv::Mat image = cv::imread(rdr_pThumbnailFile, cv::IMREAD_COLOR);
    if (image.data == NULL) {
        std::cerr << "\ncv::imread : error reading image \"" << rdr_pThumbnailFile << "\"." << std::endl;
	std::exit(-1);
    }


    if (!silent_process) std::cout << "Cropping blacks." << std::endl;
    unsigned iw = image.size().width;
    unsigned ih = image.size().height;
    unsigned ct = 0;
    unsigned cb = ih;
    unsigned cl = 0;
    unsigned cr = iw;
    float edge = 0;
    for (unsigned r = 0; r < ih; r++) {
        cv::Mat row = cv::Mat(image, cv::Rect(0, r, iw, 1)); //cv::Rect(x,y,width,height)
        cv::Scalar m = cv::mean(row);
        float e = std::max(std::max(std::max(m[0], m[1]), m[2]), m[3]);
        if (e != 0) {
            ct = r;
            edge = edge + e;
            break;
        }
    }
    for (int r = ih - 1; r >= 0; r--) {
        cv::Mat row = cv::Mat(image, cv::Rect(0, r, iw, 1));
        cv::Scalar m = cv::mean(row);
        float e = std::max(std::max(std::max(m[0], m[1]), m[2]), m[3]);
        if (e != 0) {
            cb = static_cast<unsigned>(r) + 1;
            edge = edge + e;
            break;
        }
    }
    for (unsigned c = 0; c < iw; c++) {
        cv::Mat column = cv::Mat(image, cv::Rect(c, 0, 1, ih));
        cv::Scalar m = cv::mean(column);
        float e = std::max(std::max(std::max(m[0], m[1]), m[2]), m[3]);
        if (e != 0) {
            cl = c;
            edge = edge + e;
            break;
        }
    }
    for (int c = iw - 1; c >= 0; c--) {
        cv::Mat column = cv::Mat(image, cv::Rect(c, 0, 1, ih));
        cv::Scalar m = cv::mean(column);
        float e = std::max(std::max(std::max(m[0], m[1]), m[2]), m[3]);
        if (e != 0) {
            cr = static_cast<unsigned>(c) + 1;
            edge = edge + e;
            break;
        }
    }
//    std::cout << "Width: " << cl << "><" << iw - cr << ", height: " << ct << "><" << ih - cb << std::endl;
    cl = std::min(cl, iw - cr);
    ct = std::min(ct, ih - cb);
//    std::cout << "edge: " << edge << std::endl;
    if (edge < 50.0) { // 12.5 * [left, right, top, bottom]
        cl = static_cast<unsigned>(std::max(0, static_cast<int>(cl) - 17));
        ct = static_cast<unsigned>(std::max(0, static_cast<int>(ct) - 17));
    }
    if (!silent_process) std::cout << "Horisontal borders: " << cl << ", vertical borders: " << ct << std::endl;
    image = cv::Mat(image, cv::Rect(cl, ct, iw - cl * 2, ih - ct * 2)); //cv::Rect(x,y,width,height)

// /*
    if (!silent_process) std::cout << "Downscaling to 100x80 pixels." << std::endl;
    cv::Size imageSize = image.size();
    double Xds = 100.0 / static_cast<double>(imageSize.width);
    double Yds = 80.0 / static_cast<double>(imageSize.height);
    if (Xds <= Yds) {
	imageSize.width  = static_cast<int>(static_cast<double>(imageSize.width)  * Xds);
	imageSize.height = static_cast<int>(static_cast<double>(imageSize.height) * Xds);
    } else {
	imageSize.width  = static_cast<int>(static_cast<double>(imageSize.width)  * Yds);
	imageSize.height = static_cast<int>(static_cast<double>(imageSize.height) * Yds);
    }
    cv::resize(image, image, imageSize * 32, 0, 0, cv::INTER_LANCZOS4); //cv::INTER_LANCZOS4 cv::INTER_CUBIC cv::INTER_AREA cv::INTER_LINEAR
    cv::resize(image, image, imageSize,      0, 0, cv::INTER_AREA);
// */
    cv::imencode(".jpg", image, jpeg_buff, OpenCV_JPEG_options);
    thumb_size = jpeg_buff.size();


// -----------------------------------------------------------------------------
    // checking extension
    std::string drawingExt = rdr_pInputFile;
    size_t tailSlash = drawingExt.find_last_of('/');
    size_t tailBackslash = drawingExt.find_last_of('\\');
    //std::cout << "tailSlash, tailBackslash, npos: " << tailSlash << ", " << tailBackslash << ", " << drawingExt.npos << std::endl;
    if (tailSlash == drawingExt.npos) tailSlash = tailBackslash;
    if (tailBackslash != drawingExt.npos) if (tailBackslash > tailSlash) tailSlash = tailBackslash;
    if (tailSlash != drawingExt.npos) drawingExt.erase(0, tailSlash + 1);
    //std::cout << "name: " << drawingExt << std::endl;
    size_t tailDot = drawingExt.find_last_of('.');
    if (tailDot != drawingExt.npos) {
        drawingExt.erase(0, tailDot);
    } else {
err_unknown_extension:
        std::cerr << "\nUnknown extension of \"" << rdr_pInputFile << "\".\nValid are .rdr, .geerdr, .drawinghand" << std::endl;
	std::exit(-1);
    }
    for (std::string::size_type i = 0; i < drawingExt.length(); i++)
        drawingExt[i] = tolower(drawingExt[i]);
    //std::cout << "extension: " << drawingExt << std::endl;
    if (tailSlash != drawingExt.npos) tailDot = tailSlash + tailDot + 1;
    //std::cout << "at position: " << tailDot << std::endl;
    if (drawingExt == drawingExt1) { drawingType = 1; }       // 1 rdr  2 geerdr  3 drawinghand
    else if (drawingExt == drawingExt2) { drawingType = 2; }
    else if (drawingExt == drawingExt3) { drawingType = 3; }
    else goto err_unknown_extension;


// -----------------------------------------------------------------------------
    if (!silent_process) std::cout << "Reading the drawing: \"" << rdr_pInputFile << "\"." << std::endl;
    std::ifstream drawing (rdr_pInputFile, std::ifstream::binary);
    if (!drawing) {
drawing_error_msg:
        std::cerr << "\nError opening the drawing \"" << rdr_pInputFile << "\"." << std::endl;
	std::exit(-1);
    }
    drawing.seekg (0, drawing.end); // get length of drawing file
    int drawing_length = drawing.tellg();
    if ((drawing_length < 0) or (drawing_length > 100000000)) {   // limit to 100 Mb
        drawing.close();
        goto drawing_error_msg;
    }
    drawing.seekg (0, drawing.beg);
    drawing_buffer = new char [drawing_length]; // allocate memory
    drawing.read (drawing_buffer, drawing_length); // read data as a block
    drawing.close();


// -----------------------------------------------------------------------------
    unsigned data_pos = 0;

if (drawingType == 1) {
    //.rdr
    if (memcmp(drawing_buffer, &rdr_new_header, sizeof(rdr_new_header)) == 0) {
        unsigned tl = *((unsigned*) (drawing_buffer + sizeof(rdr_new_header)));
        // std::cout << "thumbnail length = " << tl << std::endl;
        data_pos = data_pos + sizeof(rdr_new_header) + 4 + tl + 14;
        if (data_pos + sizeof(rdr_data) + 4 < static_cast<unsigned>(drawing_length)) {
            if (memcmp(drawing_buffer + data_pos, &rdr_data, sizeof(rdr_data)) == 0) {
                if (drawing_buffer[data_pos + sizeof(rdr_data) + 3] == '\4') {
                    if (!force_drawing || !silent_process)
                        std::cout << "The drawing \"" << rdr_pInputFile << "\" already contains thumbnail (" << tl << " bytes)." << std::endl;
                    if (!force_drawing) {
                        std::cout << "Skipped." << std::endl;
                        goto finish_drawing;
                    }
                }
            }
        }
    }

    while (data_pos < static_cast<unsigned>(drawing_length)) {
        data_pos = std::find(drawing_buffer + data_pos, drawing_buffer + drawing_length, rdr_data[0]) - drawing_buffer;
//        if ((data_pos & 0xff) == 0)
//            std::cout << "data_pos: " << data_pos << std::endl;

        if (memcmp(drawing_buffer + data_pos, &rdr_data, sizeof(rdr_data)) == 0) {
            if (drawing_buffer[data_pos + sizeof(rdr_data) + 3] == '\4') {
                short w1 = *((short*) (drawing_buffer + data_pos - 12));
                short h1 = *((short*) (drawing_buffer + data_pos - 10));
                if ((h1 == 0) or (w1 == 0)) {
                    std::cout << "\nWarning: zero width or height found." << std::endl;
                    if (!silent_process) {
                        std::cout << "Is it really a drawing?\n" << std::endl;
                    } else {
                        std::cout << "Is \"" << rdr_pInputFile << "\" really a drawing?" << std::endl;
                    }
                }
                data_pos = data_pos - 14;
                goto write_new_rdr;
            }
        } else
            data_pos++;
    }
    std::cout << "\nDrawing data is not found in \"" << rdr_pInputFile << "\".\nCan not make valid drawing file." << std::endl;
    goto finish_drawing;

} else if (drawingType == 2) {
    //.geerdr
    std::cout << "Extension .geerdr is not supported yet." << std::endl;
    goto finish_drawing;

} else if (drawingType == 3) {
    //.drawinghand
    std::cout << "Extension .drawinghand is not supported yet." << std::endl;
    goto finish_drawing;

} else goto finish_drawing;


// -----------------------------------------------------------------------------
write_new_rdr:
    if (rdr_pOutputFile == "")
        rdr_pOutputFile = rdr_pInputFile.substr(0, tailDot) + ".thumb" + drawingExt;
    if (!silent_process) std::cout << "Writing new drawing: \"" << rdr_pOutputFile << "\"." << std::endl;
    outfile = std::ofstream(rdr_pOutputFile, std::ofstream::binary);
    if (!outfile) {
        delete[] drawing_buffer;
        std::cerr << "\nError writing the drawing \"" << rdr_pOutputFile << "\"." << std::endl;
	std::exit(-1);
    }
    outfile.write (&rdr_new_header[0], sizeof(rdr_new_header));
    outfile.write (reinterpret_cast<const char*>(&thumb_size), 4);
    outfile.write (reinterpret_cast<const char*>(jpeg_buff.data()), thumb_size);
    outfile.write (drawing_buffer + data_pos, drawing_length - data_pos);
    outfile.close();


    // finished
finish_drawing:
    delete[] drawing_buffer;
    return 0;
}
