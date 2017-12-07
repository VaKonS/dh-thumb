# dh-thumb
Adds thumbnails to drawings for "Drawing Hand Screen Saver" by Jeff Newman (http://www.drawinghand.com).<br>
(I don't develope the screensaver, it's just a helping utility).

This software is public domain.

To compile, you'll need [OpenCV](https://github.com/opencv/opencv) and [TCLAP](https://sourceforge.net/projects/tclap/) packages, try to run "make".

---
## Usage:
<br>
`rdr.exe -i "original.geerdr"`<br>
This will try to run the screensaver, to render the drawing, and should make "original.thumb.geerdr" drawing, containing thumbnail.<br>
The program is tested with DH v2015.<br>
<br>
Options:<br>
<br>
-o&nbsp;"drawing.geerdr"<br>
Output drawing name; the backup of configuration will be written to this folder too;<br>
<br>
-t&nbsp;"image.png"<br>
Image with thumbnail (BMP/PNG/TIFF); when thumbnail is loaded from image/clipboard, the DH screensaver is not needed;<br>
<br>
-c&nbsp;1<br>
Paste thumbnail from clipboard;<br>
<br>
-s&nbsp;1<br>
Show only errors;<br>
<br>
-f&nbsp;1<br>
Process files even if they already contain drawings;<br>
<br>
-x&nbsp;"image_format"<br>
Save PNG/TIF/BMP/etc. image of cropped thumbnail together with drawing;<br>
<br>
-r&nbsp;W_H<br>
Set screen resolution for screensaver to WxH pixels (or set it manually to not switch videomodes every run);<br>
<br>
-b&nbsp;1<br>
Do not backup/restore DH's "config.dat". <b>Backup manually in that case, or settings will be broken!</b><br>
<br>
Supported drawings formats are "*.rdr", "*.geerdr" and "*.drawinghand".

---
## По-русски (in Russian):
Программа предназначена для вставки миниатюр в рисунки для хранителя экрана "Drawing Hand" (у многих старых рисунков они отсутствуют).<br>
<br>
Для этого можно запустить:<br>
`rdr.exe -i "оригинальный_рисунок.geerdr"`<br>
<br>
Программа попытается вызвать хранитель экрана и сделать рисунок "оригинальный_рисунок.thumb.geerdr", содержащий миниатюру.<br>
Проверялась с DH v2015.<br>
<br>
Настройки:<br>
<br>
-o&nbsp;"новый_рисунок.geerdr"<br>
Задать имя нового рисунка; в папку с ним также будет резервироваться файл настроек DH "config.dat";<br>
<br>
-t&nbsp;"миниатюра.png"<br>
Изображение с законченным рисунком (BMP/PNG/TIFF); если указано, хранитель экрана не запускается и не требуется;<br>
<br>
-c&nbsp;1<br>
Вставить миниатюру, предварительно скопированную в буфер обмена;<br>
<br>
-s&nbsp;1<br>
Не писать в консоль ничего, кроме сообщений об ошибках;<br>
<br>
-f&nbsp;1<br>
Делать новые рисунки, даже если старые уже содержат миниатюры;<br>
<br>
-x&nbsp;"формат_изображения"<br>
Сохранить миниатюру с обрезанным фоном в формате PNG/TIF/BMP/т.п. вместе с обработанным рисунком;<br>
<br>
-r&nbsp;Ш_В<br>
При запуске хранителя экрана установить разрешение экрана ШxВ пикселей (либо, чтобы видеорежимы не скакали, установите его заранее);<br>
<br>
-b&nbsp;1<br>
Не резервировать файл конфигурации. В этом случае сделайте копию настроек вручную, <b>файл с настройками будет повреждён!</b><br>
<br>
Поддерживаются форматы рисунков "\*.rdr", "\*.geerdr" и "\*.drawinghand".

---
![Resulting thumbnail](https://github.com/VaKonS/dh-thumb/raw/master/admir.gif)
