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

const char verHeader[] = {'\x08', 'V','e','r','s','i','o','n'}; //offset 4
const char ver9desc[] = {'\x04', 'd','e','s','c'};
const char geeSignature[] = {'\7','\0','C','O','p','M','o','v','e'};

const std::string drawingExt1 = ".rdr";
const std::string drawingExt2 = ".geerdr";
const std::string drawingExt3 = ".drawinghand";


// uninitialized data
unsigned thumb_size;
int drawingType; // 1 rdr  2 geerdr  3 drawinghand
int geeVersion; // 1-4 geerdr, 5-9 drawinghand
int geeSubVersion;
bool silent_process, force_drawing;
std::string rdr_pInputFile, rdr_pThumbnailFile, rdr_pOutputFile, drawingExt;
std::vector<uchar> jpeg_buff;
std::ofstream outfile;
char * drawing_buffer;
cv::Mat drawing_corners[4];
cv::Scalar corners_mean;


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


    if (!silent_process) std::cout << "Cropping." << std::endl;
    unsigned iw = image.size().width;
    unsigned ih = image.size().height;
    unsigned ct = 0;
    unsigned cb = ih;
    unsigned cl = 0;
    unsigned cr = iw;
    float edge = 0;

    // cv::Vec3f image.at(ih-1,iw-1) crashes, therefore using cv::Mat
    drawing_corners[0] = cv::Mat(image, cv::Rect(0, 0, 1, 1)); //cv::Rect(x,y,width,height)
    drawing_corners[1] = cv::Mat(image, cv::Rect(iw - 1, 0, 1, 1));
    drawing_corners[2] = cv::Mat(image, cv::Rect(0, ih - 1, 1, 1));
    drawing_corners[3] = cv::Mat(image, cv::Rect(iw - 1, ih - 1, 1, 1));
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
    if (edge < 20.0) { // 5.0 * [left, right, top, bottom]
        if (!silent_process) std::cout << "Edges are not solid (" << edge / 4 << " < 5), some borders left." << std::endl;
        cl = static_cast<unsigned>(std::max(0, static_cast<int>(cl) - 17));
        ct = static_cast<unsigned>(std::max(0, static_cast<int>(ct) - 17));
    }
    //if (!silent_process) std::cout << "Horisontal crop: " << cl << ", vertical crop: " << ct << " (" << iw - cl * 2 << "x" << ih - ct * 2 << ")." << std::endl;
    image = cv::Mat(image, cv::Rect(cl, ct, iw - cl * 2, ih - ct * 2)); //cv::Rect(x,y,width,height)
    //cv::imwrite(rdr_pThumbnailFile + ".png", image);


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


    if (rdr_pOutputFile == "")
        rdr_pOutputFile = rdr_pInputFile.substr(0, tailDot) + ".thumb" + drawingExt;


// -----------------------------------------------------------------------------
    unsigned data_pos = 0;
    unsigned w1 = 0;
    unsigned h1 = 0;

if (drawingType == 1) {
    //.rdr
// NN NN ................................................. NN NN WWWW HHHH NN NN NN NN NN NN NN NN 00 03 00 01 palette 04 ...
// 2f 00 01 00 00 00 01 00 00 00 00 00 00 00 LLLLLLLL jpeg NN NN WWWW HHHH NN NN NN NN NN NN NN NN 00 03 00 01 palette 04 ...

    unsigned tl;
    if (memcmp(drawing_buffer, &rdr_new_header, sizeof(rdr_new_header)) == 0) {
        tl = *((unsigned*) (drawing_buffer + sizeof(rdr_new_header)));
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
    } else
        tl = 0;

    // tl = search limit
    tl = std::min(tl + 1000000, static_cast<unsigned>(drawing_length) - sizeof(rdr_data));
    while (data_pos <= tl) {
        data_pos = std::find(drawing_buffer + data_pos, drawing_buffer + drawing_length - sizeof(rdr_data) + 1, rdr_data[0]) - drawing_buffer;
        if (data_pos < (drawing_length - sizeof(rdr_data) + 1)) {
            if (memcmp(drawing_buffer + data_pos, &rdr_data, sizeof(rdr_data)) == 0) {
                if (drawing_buffer[data_pos + sizeof(rdr_data) + 3] == '\4') {
                    w1 = *((short*) (drawing_buffer + data_pos - 12));
                    h1 = *((short*) (drawing_buffer + data_pos - 10));
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
                        delete[] drawing_buffer;
                        std::cerr << "\nError writing the drawing \"" << rdr_pOutputFile << "\"." << std::endl;
	                std::exit(-1);
                    }
                    outfile.write (&rdr_new_header[0], sizeof(rdr_new_header));
                    outfile.write (reinterpret_cast<const char*>(&thumb_size), 4);
                    outfile.write (reinterpret_cast<const char*>(jpeg_buff.data()), thumb_size);
                    outfile.write (drawing_buffer + data_pos, drawing_length - data_pos);
                    outfile.close();

                    goto finish_drawing;
                }
            } else
                data_pos++;
            //if (data_pos > prev_pos) { std::cout << "data_pos: " << data_pos << std::endl; prev_pos = data_pos + 1000; }
        } else
            break; // pattern was not found
    }

    std::cout << "\nDrawing data is not found in \"" << rdr_pInputFile << "\".\nCan not make valid drawing file." << std::endl;

} else if( (drawingType == 3) || (drawingType == 2) ){
    //.drawinghand, .geerdr
// 0  1  2  3  4   56789012  13 14 15 16 17 18 19 20 21  2345  22/26         0        4        8  9  10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29
// VV 00 00 00 08 "Version9" NN 00 00 00 01 00 00 00 04 "desc" LLLLLLLL bmp  WWWWWWWW HHHHHHHH ff ff ff 00 02 00 00 00 ff ff ff ff 00 ac 5e 00 00 ff ff 00 00 07 00 COpMove ...
// VV 00 00 00 08 "Version8" NN 00 00 00 01 00 00 00 00        LLLLLLLL jpeg WWWWWWWW HHHHHHHH ff ff ff 00 02 00 00 00 ff ff ff ff 00 ac 5e 00 00 ff ff 00 00 07 00 COpMove ...
// VV 00 00 00 08 "Version7"                                                 WWWWWWWW HHHHHHHH ff ff ff 00 02 00 00 00 ff ff ff ff 00 ac 5e 00 00 ff ff 00 00 07 00 COpMove ...
// 01 02? 03? 04 - geerdr, 05 06 - drawinghand
//                           NN 00 00 00                                     WWWWWWWW HHHHHHHH ff ff ff 00 02 00 00 00 ff ff ff ff 00 ac 5e 00 00 ff ff 00 00 07 00 COpMove ...

    if (memcmp(drawing_buffer + 4, &verHeader, sizeof(verHeader)) == 0) {
        //std::cout << "found VersionX header" << std::endl;
        geeVersion = drawing_buffer[4 + sizeof(verHeader)] - '0'; //ver=8,9,?
        //if (!silent_process) std::cout << "Geerdr version: " << geeVersion << std::endl;
        if (drawing_buffer[0] == geeVersion) {
            //std::cout << "Header matches geeVersion." << std::endl;
        } else {
std::cout << "Header version does not match 'Version'. Stop." << std::endl;
            goto finish_drawing;
        }
        if (geeVersion == 7) {
            // Version 7 seems to not support thumbnails, changing to version 8, subversion 7.
            geeVersion = 8;
            geeSubVersion = 7;
            data_pos = 13;
            goto headerless_geerdr;
        }
        geeSubVersion = *((int*)(drawing_buffer + 13));
        //if (!silent_process) std::cout << "Geerdr subversion: " << geeSubVersion << std::endl;
        data_pos = 22;
        if (memcmp(drawing_buffer + 21, &ver9desc, sizeof(ver9desc)) == 0) {
            if (geeVersion != 9) {
std::cout << "'desc' found, but Version is not 9. Stop." << std::endl;
                goto finish_drawing;
            }
            data_pos = data_pos + 4;
        }
        unsigned tl = *((unsigned*)(drawing_buffer + data_pos));
        //std::cout << "Thumbnail length: " << tl << " bytes." << std::endl;

        // checking thumbnail format
        short thumbFmt = *((short*)(drawing_buffer + data_pos + 4));
        if (thumbFmt == 0x4d42) { // "BM" - bmp thumbnail
            //if (!silent_process) std::cout << "BMP thumbnail." << std::endl;
            //if (geeVersion != 9) {
            //    //std::cout << "geeVersion is not 9, but thumbnail is BMP! Stop." << std::endl;
            //    goto finish_drawing;
            //}
        } else if (thumbFmt == -9985) { // 0xd8ff - jpeg thumbnail
            //if (!silent_process) std::cout << "JPEG thumbnail." << std::endl;
            //if (geeVersion == 9) {
            //    std::cout << "JPEG thumbnail in Version9! Stop." << std::endl;
            //    goto finish_drawing;
            //}
        } else {
            std::cout << "Unknown thumbnail format (" << thumbFmt << ").\nOnly BMP and JPEG are supported. Stop." << std::endl;
            goto finish_drawing;
        }

        data_pos = data_pos + 4 + tl;
        w1 = *((unsigned*) (drawing_buffer + data_pos));
        h1 = *((unsigned*) (drawing_buffer + data_pos + 4));
        //if (!silent_process) std::cout << "Canvas size: " << w1 << "x" << h1 << " pixels." << std::endl;
        if ((h1 == 0) or (w1 == 0)) {
            std::cout << "\nWarning: zero width or height found." << std::endl;
            if (!silent_process) {
                std::cout << "Is it really a drawing?\n" << std::endl;
            } else {
                std::cout << "Is \"" << rdr_pInputFile << "\" really a drawing?" << std::endl;
            }
        }
        if (memcmp(drawing_buffer + data_pos + 29, &geeSignature, sizeof(geeSignature)) == 0) {
            //std::cout << "geeSignature found." << std::endl;
            if (!force_drawing || !silent_process)
                std::cout << "The drawing \"" << rdr_pInputFile << "\" already contains thumbnail (" << tl << " bytes)." << std::endl;
            if (!force_drawing) {
                std::cout << "Skipped." << std::endl;
                goto finish_drawing;
            }
        } else
            goto no_gee_sig;
    } else { // no "VersionX" header - versions 1...6, changing to version 8, subversion N
        geeVersion = 8;
        geeSubVersion = *((int*) drawing_buffer);
        data_pos = 4;
headerless_geerdr:
        //if (!silent_process) std::cout << "No thumbnail." << std::endl;
        w1 = *((int*) (drawing_buffer + data_pos));
        h1 = *((int*) (drawing_buffer + data_pos + 4));
        //if (!silent_process) std::cout << "Canvas size: " << w1 << "x" << h1 << " pixels." << std::endl;
        if ((h1 == 0) or (w1 == 0)) {
            std::cout << "\nWarning: zero width or height found." << std::endl;
            if (!silent_process) {
                std::cout << "Is it really a drawing?\n" << std::endl;
            } else {
                std::cout << "Is \"" << rdr_pInputFile << "\" really a drawing?" << std::endl;
            }
        }
        if (memcmp(drawing_buffer + data_pos + 29, &geeSignature, sizeof(geeSignature)) != 0) {
no_gee_sig:
            std::cout << "\nDrawing data is not found in \"" << rdr_pInputFile << "\".\nCan not make valid drawing file." << std::endl;
            goto finish_drawing;
        } //else {
         //   std::cout << "geeSignature found." << std::endl;
        //}
    }

    if ( (geeVersion != 8) and (geeVersion != 9) ) {
        std::cout << "\n'Version' is not 8 or 9. Stop." << std::endl;
        goto finish_drawing;
    }

    if (!silent_process) std::cout << "Writing new drawing: \"" << rdr_pOutputFile << "\"." << std::endl;
    outfile = std::ofstream(rdr_pOutputFile, std::ofstream::binary);
    if (!outfile) {
        delete[] drawing_buffer;
        std::cerr << "\nError writing the drawing \"" << rdr_pOutputFile << "\"." << std::endl;
	std::exit(-1);
    }
    outfile.write (reinterpret_cast<const char*>(&geeVersion), 4);
    outfile.write (&verHeader[0], sizeof(verHeader));
    int geeVersionL = geeVersion + '0';
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
    outfile.write (drawing_buffer + data_pos, drawing_length - data_pos);
    outfile.close();

}


// - finished ------------------------------------------------------------------
finish_drawing:
    delete[] drawing_buffer;
    if ((iw < w1) || (ih < h1)) {
        std::cout << "\nImage dimensions (" << iw << "x" << ih << ") are smaller than drawing canvas (" << w1 << "x" << h1 << ")."
                     "\nThe thumbnail in \"" << rdr_pOutputFile << "\" is cropped." << std::endl;
    }
    return 0;
}
