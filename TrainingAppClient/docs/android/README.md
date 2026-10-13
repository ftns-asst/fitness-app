# Android-контур демо

Целевая платформа продукта — Android. Десктоп и «рамка телефона» 390×812
(`--frame`) используются только для дизайн-ревью и скриншотов; живая проверка
UI проводится на эмуляторе/устройстве.

## Сборка и запуск

- Kit Qt Creator: `Qt 6.11.2 for Android x86_64` (Debug), каталог сборки
  `build/Qt_6_11_2_for_Android_x86_64_Debug`.
- Эмулятор: `emulator-5554` (x86_64). Запуск из Android Studio/AVD Manager или
  `emulator -avd <имя>`.
- Деплой из Qt Creator (Run) или вручную:
  `adb -s emulator-5554 install -r build\Qt_6_11_2_for_Android_x86_64_Debug\android-build-appTrainingAppClient\build\outputs\apk\debug\android-build-appTrainingAppClient-debug.apk`.
- Собственный пакет: `android/AndroidManifest.xml` (подключён через
  `QT_ANDROID_PACKAGE_SOURCE_DIR`). Отличия от шаблона Qt: portrait-lock
  activity, label «Фитнес-помощник», versionName из `project(VERSION)`.

## Скриншоты с устройства

```powershell
adb -s emulator-5554 exec-out screencap -p > docs\ui-preview\android-<имя>.png
```

Снимать после стабилизации анимаций (~1 с). Хранить в `docs/ui-preview/`
с префиксом `android-`, чтобы не смешивать с десктопными framed-снимками.

## Мобильный чек-лист этапа

1. Insets: контент не залезает под системный status bar и gesture-полосу;
   tab bar стоит над системным низом (`SafeArea.margins` в `qml/Main.qml`).
2. Навигация: системная «назад» закрывает Drawer/стек деталей по приоритетам
   (`BackHandler` в `qml/AppShell.qml`); на вкладках — выход из приложения.
3. Тема: в тёмной теме системные иконки полос светлые и читаемые
   (`PlatformChrome.applyChrome` — QML-имя класса `APlatform_Chrome` из
   `src/Platform/Platform_Chrome.*`; JNI `WindowInsetsController`, API 30+;
   на старых API — no-op без краша).
4. Touch: tap-цели не меньше `Theme.touchMin` (44 dp); chips — hit-area 44 dp
   при визуальных 36 dp; скролл — транзиентный `ScrollIndicator`, без
   десктопного скроллбара.
5. IME: открытая клавиатура (поиск, заметка к подходу) не перекрывает CTA
   (activity в режиме adjustResize).
6. Orientation: поворот эмулятора не уводит приложение в landscape.
7. Читаемость: тексты caption/body различимы при плотности эмулятора;
   одноколоночный layout не ломается на узких экранах (360 dp).

## Ограничения демо

- Размеры в dp, не sp: системный font scale не масштабирует тексты
  (продуктовое решение примет этап M2+).
- Portrait-only; планшеты/landscape — вне scope демо.
- Иконка приложения и splash — дефолтные Qt; брендирование на этапе M2.
- Без хаптики и системных звуков; без edge-to-edge рисования под полосы
  (Qt 6.9+ и так edge-to-edge, отступы учтены через `SafeArea`).
- Status bar mock («9:41 / LTE / 87%», «офлайн») рисуется только в framed-режиме
  дизайн-ревью; на устройстве системный status bar не перекрываем — состояние
  «офлайн» во время записи тренировки показывается внутри оверлея (этап D2).
