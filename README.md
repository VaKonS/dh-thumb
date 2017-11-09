# dh-thumb
Adds thumbnails to drawings for "Drawing Hand Screen Saver" by Jeff Newman (http://www.drawinghand.com).<br>
(I don't develope the screensaver, it's just a helping utility).

This software is public domain.

To compile, you'll need [OpenCV](https://github.com/opencv/opencv) and [TCLAP](https://sourceforge.net/projects/tclap/) packages, try to run "make".

---
## Usage:
1. Run the Drawing Hand screensaver, when drawing is finished, press "Print Screen" button.<br>
Run:<br>
`rdr.exe -i "original.geerdr" -c 1`<br>
This will take the screenshot from clipboard.
2. Or you can paste the screenshot into some graphical program, i. e. MS Paint, and save it to "rendered_image.png".<br>
Acceptable formats are PNG, BMP, TIFF and some other.<br>
*Do not use JPEG and other lossy formats, because they will not be cropped properly.*<br>
Then run:<br>
`rdr.exe -i "original.drawinghand" -t "rendered_image.png"`<br>
<br>
Supported drawings formats are "*.rdr", "*.geerdr" and "*.drawinghand".

---
## По-русски (in Russian):
Программа предназначена для вставки миниатюр в рисунки для хранителя экрана "Drawing Hand" (у многих старых рисунков они отсутствуют).<br>
Для этого можно:
1) Сделать снимок экрана клавишей "Print Screen" и запустить:<br>
`rdr.exe -i "оригинал.geerdr" -c 1`<br>
Картинка будет вставлена из буфера обмена.
2) Либо сохранить снимок экрана в файл PNG/BMP/TIFF.<br>
*Не сохраняйте в формат с потерями (JPEG и т. п.)&nbsp;– не сработает авто-обрезка.*<br>
Запустить:<br>
`rdr.exe -i "оригинал.drawinghand" -t "скриншот.png"`<br>
<br>
Поддерживаются форматы рисунков "*.rdr", "*.geerdr" и "*.drawinghand".

---
![Resulting thumbnail](https://github.com/VaKonS/dh-thumb/raw/master/admir.gif)
