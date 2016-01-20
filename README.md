# streetmangler - реестр и конвертер названий улиц

В российском OSM принято [соглашение о правилах именования улиц][1],
позволяющее (при соответствии ему подавляющей части базы, разумеется)
избежать значительной части адресных ошибок, неполноты данных, упростить
машинную обработку и просто использовать единый для всей страны стиль.

Изначально для проверки соответствия и приведения в соответствие
соглашению названий улиц был применён [эвристический подход][2],
позволивший поднять долю корректно именованных улиц с 55% до 96%,
одновременно сократив количество адресных ошибок по меньшей мере в 5 раз,
или на ~100000 при общем количестве адресов в базе 600000-700000 (т.е.
каждый 7-й адрес был ошибочным). Однако этот подход имеет свои
ограничения в виде процента ложных срабатываний достаточного для
необходимости ручной проверки замен, что не позволяет выложить инструмент
для исправления названий в общий доступ или выполнять исправления
регулярно в автоматическом режиме.

Альтернативным решением является ведение словаря или реестра всех
существующих названий улиц. Это позволит полностью избежать ложных
срабатываний (помимо действительных ошибок в словаре), хотя и потребует
значительной работы по наполнению базы, но если учесть что в
эвристическом нормализаторе также используются словари (для определения
частей речи) и он также требует ручной работы по проверке данных, новый
подход видится гораздо более эффективным.

Также автор надеется что проект найдёт применение и за пределами OSM.

[1]: http://wiki.openstreetmap.org/wiki/RU:ВикиПроект_Россия/Соглашение_об_именовании_дорог
[2]: http://forum.openstreetmap.org/viewtopic.php?id=11930

## Возможности

На данный момент реализовано:
* поиск точного совпадения
* поиск неточного совпадения (разный регистр, разный порядок слов,
  сокращения статусной части)
* поиск неточного совпадения с учётом орфографических ошибок
* поиск названий с предположительно отсутствующей статусной частью
* класс для обработки названий улиц, который можно использовать
  в том числе и отдельно от базы, например в конверторах
* утилита для выделения названий улиц из OSM XML дампа, и их
  классификации

Пока не реализовано:
* поддержка других языков (хотя Komzpa использует белорусскую
  локаль)
* утилита для самотестирования (сверка базы с ней же для поиска
  потенциальных ошибок)
* утилита для замены названий улиц в OSM
* утилиты для сверки с другими источниками, например КЛАДР

## База

Данные берутся из OpenStreetMap. Используются все тэги addr:street
и аналогичные (addr:street2, addr2:street и т.д.), name линейных
объектов с тегом highway, за исключением bus_stop и
emergency_access_point, а также name отношений указанных типов.

Формат базы тривиален - текстовый файл в кодировке UTF-8, по одному
названию улицы на строку, текст после символа # считается
комментарием и игнорируется, равно как и лишние пробелы.

Смотри [data/ru.txt](data/ru.txt)

## Библиотека

Исходный код из директории lib/ может использоваться как с, так
и отдельно от базы. Смотри [README.API](README.API)

## Биндинги

Имеются swig-биндинги для python и perl, поддерживающие большую
часть функционала библиотеки.

## Сборка

Необходимые зависимости:
* cmake
* icu
* expat2

Опциональные зависимости:
* perl (для perl биндингов)
* python (для python биндингов)
* swig (для любых биндингов)

Сборка:
```
cmake . && make
```

Сборка без биндингов:
```
cmake -DWITH_PERL=NO -DWITH_PYTHON=NO . && make
```

Запуск тестов:
```
ctest -V
```

## Утилиты

Единственная на данный момент утилита - process_names. Она позволяет
загрузить из текстового файла или выбрать из OSM XML дампа названия
улиц, сопоставить их с базой и классифицировать. По результатам
можно получить статистику и списки найденных/не найденных улиц,
которые можно далее использовать как для пополнения базы, так и
для исправления названий в OSM.

Формат вызова:

```
process_names [-cdhsAN] [-p N] [-l locale] [-a tag] [-n tag] [-r type] [-f database] file.osm|file.txt|- ...
```

Аргументами может быть любое число файлов с расширениями .osm
(обрабатывается как osm xml дамп), .txt (обрабатывается как
текстовый список названий), а также символ -, который означает
чтение osm xml дампа с stdin.

Опции:

```-s``` дополнительно к статистике по уникальным названиям улиц
         считать статистику по каждому отдельному использованию
         названия. Немного замедляет работу утилиты.

```-d``` сохранять списки улиц в файлы в текущем каталоге:

* ```dump.all.txt``` -
  все названия из входного файла

* ```dump.exact_match.txt``` -
  точные совпаденя с базой

* ```dump.canonical_form.txt``` -
  названия требующие приведения к канонической форме

* ```dump.spelling_fixed.txt``` -
  предположительно исправления опечаток

* ```dump.stripped_status.txt``` -
  названия с пропущенной статусной частью

* ```dump.no_match.txt``` -
  названия не найденные в базе

* ```dump.no_match.full.txt``` -
  названия не найденные в базе, приведённые к полной
  форме (т.е. в формат подходящий для пополнения базы)

* ```dump.non_names.txt``` -
  предположительно не названия улиц

```-c``` при использовании опции -d также создавать списки названий
         улиц с количеством раз, сколько каждое название встречалось

* ```dump.counts.all.txt``` -
  все названия

* ```dump.counts.spelling_fixed.txt``` -
  предположительно исправления опечаток

* ```dump.counts.no_match.txt``` -
  названия не найденные в базе

* ```dump.counts.non_name.txt``` -
  предположительно не названия улиц

```-l``` указать локаль (по умолчанию и единственная доступная на
         данный момент - "ru_RU")

```-p``` расстояние проверки орфографии (максимальное число ошибок
         в слове) (по умолчанию 1)

```-f``` указать путь к базе данных (по умолчанию используется
         data/ru.txt из директории с исходниками проекта). Можно
         использовать эту опцию несколько раз, загружная несколько
         баз

```-a``` указать тэг(и), из которых будут читаться адресные названия
	     улиц (опцию можно указывать несколько раз). По умолчанию
	     (если данная опция не указана) используются "addr:street",
	     "addr:street1", "addr:street2", "addr:street3", "addr2:street",
	     "addr3:street".

```-A``` не использовать умолчальных список адресных тэгов

```-n``` указать тэг(и), из которых будут читаться названия у
         highway-объектов (опцию можно указывать несколько раз),
         По умолчанию (если данная опция не указана) используется
         только тэг "name"

```-N``` не использовать умолчальных список name-тэгов

```-r``` указать тип(ы) отношений (тип - значение тэга type=), для
	     которых будут использоваться name тэги. (addr тэги
	     всегда обрабатываются для всех отношений)

```-h``` показать краткую справку

## Пополнение базы

Прежде всего, планируется постоянное пополнение базы данных. Эту
работу можно условно разделить на 3 фазы:

1. Доведение процента распознанных улиц до показателей
   эвристического нормализатора
2. Добавление в базу всех улиц, присутствующих в OSM
3. Периодический импорт новых названий, появляющихся в OSM

На данный момент проект находится на 1 фазе, совпадают с базой
61.52% уникальных названий улиц в России. Уровень эвристического
нормализатора - 77.40%.

Возможно, для облегчения 3 фазы будет создана отдельная база с
не-улицами (и возможно она будет использована для исправления OSM,
потому что "name=грунтовка" это, очевидно, ошибка).

В будущем возможно введение более развитой структуры базы, например
с учётом различных написаний имён собственных в названиях (т.е.
"улица Льва Толстого" vs. "улица Л.Н.Толстого") (Komzpa@)

## Проверочные источники

* Москва:
  * [Общемосковский классификатор улиц Москвы]
    (http://www.mosclassific.ru/mClass/omkum_view.php)
  * [Имена московских улиц. Топонимический словарь]
    (http://slovari.yandex.ru/~книги/Московские%20улицы/)

* Санкт-Петербург:
  * [Реестр названий объектов городской среды Санкт-Петербурга]
    (http://spbculture.ru/ru/registry.html)
  * [Петербургская топонимика]
    (http://slovari.yandex.ru/~книги/Петербургская%20топонимика/)

* Архангельск:
  * [Овсянкин Е.И., Имена Архангельских улиц]
    (http://lit.lib.ru/o/owsjankin_e_i/text_0080.shtml)
  * [Овсянкин Е.И.  Имена Архангельских улиц. Часть 2. Топонимия города.]
    (http://lit.lib.ru/o/owsjankin_e_i/text_0170.shtml)
  * [Энциклопедия улиц Архангельска]
    (http://arkh-street.ru)

* Владивосток:
  * [VladCity.com: Улицы Владивостока]
    (http://vladcity.com/streets-of-vladivostok/)

* Грязовец:
  * [Грязовец.ру]
    (http://gryazovets.ru/Категория:Улицы_Грязовца)

* Иваново:
  * [Интернет-энциклопедия Ивановсой области]
    (http://wiki.ivanovoweb.ru/index.php/Заглавная_страница)

* Ирбит:
  * ["Библиотечкая система" г.Ирбит, улицы Ирбита]
    (http://biblio-irbit.ru/index.php/kraevedenie/ulitsy-irbita)

* Иркутск:
  * [Ценрализованная библиотечная система города Иркутска: Улицы Иркутска]
    (http://cbs.irkutsk.ru/street.htm)

* Нижний Новгород:
  * [Нижегородская энциклопедия]
    (http://www.nnov.org/Главная_страница)

* Орск:
  * [Историческая страница Орска: улицы]
    (http://history.opck.org/ulitsy.html)

* Осташков:
  * [Улицы города Осташкова]
    (http://ostashkov.codis.ru/ivaul.htm)

* Ростов-на-Дону:
  * [Централизованная библиотечная система Ростова-на-Дону: улицы Ростова]
    (http://www.donlib.ru/rostov-streets.html)

* Стерлитамак:
  * [Централизованная библиотечная система г.Стерлитамак: Краеведенние]
    (http://libstr.ru/local_history), [2](http://libstr.ru/pomnit_gorod)
  * [Город-на-Стерле.Ру: История улиц]
    (http://gorod-na-sterle.ru/category/istoriya-ulic/)

* Тверь:
  * [Улицы Твери]
    (http://www.tverplanet.ru/ulitsy-tveri/) (порядок слов неправильный,
    но есть история названий)

* Томск:
  * [Товики]
    (http://towiki.ru/)

* Усть-Лабинск:
  * [wiki Усть-Лабинска]
    (http://www.ust-labinsk.ru/w/index.php/Заглавная_страница)

* Челябинск:
  * [Энциклопедия «Челябинск»]
    (http://book-chel.ru/)

* Общее:
  * [Википедия]
    (http://wikipedia.ru) (статьи об отдельных улицах, списки улиц
    отдельных городов, статьи о персонах, событиях и местах, в честь
    которых назывались улицы, списки героев)
  * [Герои страны]
    (http://warheroes.ru/)
  * [Красные Соколы: лучшие лётчики-асы России 1914-1953 гг.]
    (http://airaces.narod.ru/)
  * КЛАДР

## Лицензия

Код распространяется под GPLv3. Полный текст лицензии находится
в файле [COPYING](COPYING).

Названия улиц взяты из OpenStreetMap:
http://www.openstreetmap.org/copyright

## Автор

Дмитрий Маракасов <amdmi3@amdmi3.ru>
