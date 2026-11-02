# Прокрутка: измерения и воспроизведение

Стенд: Windows, Qt 6.11.2 / MinGW GCC 13.1.0 x64, `offscreen`, Controls Basic,
Inter из ресурсов приложения, viewport 390×812. Каталог: 500 записей без фото
и сетевых запросов. Три прогона, прогрев 1 с, прокрутка по одному маршруту 5 с.
Сырые результаты: `scroll-results.json`.

| Показатель, медиана трёх прогонов | Исходный компонент | Оптимизированный |
|---|---:|---:|
| Создание экрана и ожидание первого рендера, мс | 135 | 16 |
| Созданные карточки при прокрутке 500 записей | 500 | 18 |
| Достижение цели колеса ±0,5 px, мс | 222 | 170 |
| P95 интервала FrameAnimation, мс | 32,5 | 31,2 |

Подготовка каталога ускорилась примерно в 8,4 раза; число карточек снизилось
на 96,4 %. Время завершения шага колеса снизилось примерно на 23 %.
Пиксельные события теперь применяют смещение синхронно, без второй анимации.
Это проверяет обработчик, а не сквозную задержку от устройства до дисплея.
В обеих версиях заполнены одинаковые справочники групп мышц и инвентаря.
Данные сняты 31 октября 2026 года. Замеры после окончательного исправления
асинхронных фильтров заменяют предварительные результаты.

Устойчивого улучшения частоты кадров этот стенд не показывает: интервалы
FrameAnimation близки и зависят от планировщика offscreen. Это интервалы
GUI-анимации, не GPU-время и не время фактического показа кадра. RAM в байтах
не измерялась: число карточек — показатель объёма созданного UI, не оценка RAM.
Результаты нельзя переносить на Android/iOS или обещать по ним 60/120 fps.

## Что изменено

- Каталог использует один GridView с header/footer, recycling и ограниченным
  cacheBuffer вместо Flow + Repeater. Поиск, фильтры, пустые состояния и
  автодогрузка сохранены; вложенного вертикального скролла нет.
- PageScaffold поддерживает отдельный Flickable-корень для виртуализованных
  экранов и продолжает отдавать правильную цель оболочке и обработчикам жестов.
- Пиксельные события колеса/тачпада не игнорируются при angleDelta=0.
  Анимация обычного колеса сокращена с 220 до 120 мс и останавливается при
  смене целевого экрана. Мобильный touch по-прежнему обрабатывает Flickable.
- Физика touch, частота экрана, глобальный render loop и DPR не форсируются.

## Запуск

Из корня TrainingAppClient, PowerShell, с Qt runtime в PATH:

```powershell
New-Item -ItemType Directory -Force build/performance | Out-Null
cmake -S . -B build/refactor-desktop
$env:QT_QUICK_CONTROLS_STYLE = 'Basic'
$runner = 'D:/Qt/6.11.2/mingw_64/bin/qmltestrunner.exe'
& $runner -input tests/performance/tst_scroll_benchmark.qml -import build/refactor-desktop/tests/imports -platform offscreen -o build/performance/optimized.txt,txt
```

Для сравнения исходные каталог и обработчик берутся из c99673e; остальные
компоненты общие и одинаковы в обоих прогонах. Это сравнение двух реализаций,
не двух полных сборок приложения. Изменения рабочего дерева не откатываются:

```powershell
git -c safe.directory=E:/Projects/QML/temp-clone show c99673e:TrainingAppClient/qml/Pages/Exercises/ExercisesPage.qml | Set-Content build/performance/BaselineExercisesPage.qml -Encoding UTF8
git -c safe.directory=E:/Projects/QML/temp-clone show c99673e:TrainingAppClient/qml/Components/DragToScroll.qml | Set-Content build/performance/BaselineDragToScroll.qml -Encoding UTF8
$source = Get-Content tests/performance/tst_scroll_benchmark.qml -Raw
$source = $source.Replace('../../qml/Pages/Exercises/ExercisesPage.qml', 'BaselineExercisesPage.qml').Replace('../../qml/Components/DragToScroll.qml', 'BaselineDragToScroll.qml')
Set-Content build/performance/tst_baseline.qml $source -Encoding UTF8
& $runner -input build/performance/tst_baseline.qml -import build/refactor-desktop/tests/imports -platform offscreen -o build/performance/baseline.txt,txt
```

Начальный запуск без загрузчиков Inter не включён в итоговое сравнение.
Здесь учитываются только прогоны, проверяющие FontLoader.Ready для всех четырёх
начертаний. Benchmark отдельно от обычного QML-набора: фиксированного порога
времени в CI нет, чтобы загрузка машины не вызывала ложных отказов.

## Проверка на мобильном устройстве

Нужны release-сборка, реальные 60/120 Гц, одинаковый набор данных, длинный
каталог с фотографиями, быстрые и медленные диагональные жесты, смена экранов
во время инерции, фон/возврат. Проверить GUI/render thread и создание
делегатов в QML Profiler; отдельно снять время кадров на устройстве и память.
`--fps` и существующая плашка FPS принудительно поддерживают непрерывный
рендеринг, поэтому их следует выключать при измерении простоя/энергопотребления.

Справка Qt: [Performance Considerations](https://doc.qt.io/qt-6/qtquick-performance.html),
[GridView](https://doc.qt.io/qt-6/qml-qtquick-gridview.html),
[WheelEvent](https://doc.qt.io/qt-6/qml-qtquick-wheelevent.html).

## Проверки окончательной версии

- Сборка приложения прошла; qmllint не добавил новых предупреждений
  (существующее предупреждение FpsOverlay.refreshRate осталось).
- 260 QML-проверок прошли, включая ограниченное число делегатов на 500 строках,
  навигацию после recycling, автодогрузку, асинхронные фильтры, доступность
  заголовка через прокрутку, pixelDelta и отмену анимации при смене страницы.
- Реальный снимок приложения проверен: поиск и «+ Своё» видны после загрузки
  справочников; `build/performance/catalog-final.png`.
- Общий прогон: 133/134 наборов, все 89 UI-проверок прошли. Упал ранее
  нестабильный сетевой exercises_vm, который сбоил и до этих изменений.
  Отдельный повтор: 7/7 проверок прошли. Причина нестабильности не установлена;
  полный прогон не считается полностью зелёным.
- Журналы последнего прогона: `build/performance/ctest-final.txt`,
  `qml-final.txt`, `exercises-vm-recheck.txt`.
