# dh-thumb
Adds thumbnails to drawings for "Drawing Hand Screen Saver" by Jeff Newman (http://www.drawinghand.com).<br>
(I don't develope the screensaver, it's just a helping utility).

This software is public domain.

To compile, you'll need [OpenCV](https://github.com/opencv/opencv) and [TCLAP](https://sourceforge.net/projects/tclap/) packages, try to run "make".

---
## Usage:
1. Run the Drawing Hand screensaver, when drawing is finished, press "Print Screen" button, paste the screenshot into some graphical program, i. e. MS Paint, and save it to "rendered_image.png".<br>
Acceptable formats are PNG, BMP, TIFF and some other.<br>
*Do not use JPEG and other lossy formats, because they will not be cropped properly.*
2. Run:<br>
`rdr.exe -i "original.drawinghand" -t "rendered_image.png"`<br>

Supported drawings formats are `*.rdr`, `*.geerdr` and `*.drawinghand`.

---
## По-русски (in Russian):
Программа предназначена для вставки превьюшек в рисунки для хранителя экрана "Drawing Hand" (у многих старых рисунков они отсутствуют).<br>
Для этого нужно:
1) Сохранить снимок экрана с готовым рисунком в файл PNG/BMP/TIFF.<br>
*Не сохраняйте в формат с потерями (JPEG и т. п.)&nbsp;– не сработает авто-обрезка.*
2) Запустить `rdr.exe -i "оригинал.geerdr" -t "скриншот.png"`

---
![Resulting thumbnail](https://github.com/VaKonS/dh-thumb/raw/master/admir.gif)
