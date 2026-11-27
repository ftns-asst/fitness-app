import QtQuick
import QtQuick.Controls
import QtTest
import TrainingAppClient 1.0

TestCase {
    id: testCase

    name: "SharedMotion"
    when: windowShown
    visible: true
    width: 400
    height: 400

    Component {
        id: tabsComponent

        BottomTabBar {
            width: 360
        }
    }
    Component {
        id: comboComponent

        AppComboBox {
            width: 280
            model: [
                {
                    id: "one",
                    title: "One"
                },
                {
                    id: "two",
                    title: "Two"
                }
            ]
        }
    }
    Component {
        id: fieldComponent

        AppTextField {
            width: 280
        }
    }
    Component {
        id: tileComponent

        CatalogExerciseTile {
            width: 150
            title: "Exercise"
        }
    }

    function collectButtons(item, result) {
        for (let child of item.children) {
            if (child instanceof Button)
                result.push(child);
            else
                collectButtons(child, result);
        }
        return result;
    }
    function test_tabHighlightFinishesAfterRapidSelection() {
        const bar = createTemporaryObject(tabsComponent, testCase);
        const buttons = collectButtons(bar, []);
        compare(buttons.length, 4);
        mouseMove(testCase, 390, 390);
        const previous = buttons[0].background.children[0];
        bar.currentIndex = 1;
        // The old highlight must remain paintable while its opacity settles.
        verify(previous.visible);
        wait(30);
        bar.currentIndex = 2;
        bar.currentIndex = 0;
        tryCompare(previous, "opacity", 1);
        tryCompare(previous, "scale", 1);
        bar.currentIndex = 3;
        tryCompare(previous, "opacity", 0);
        compare(previous.visible, false);
        tryCompare(buttons[3].background.children[0], "opacity", 1);
    }
    function test_comboReopensAtFullScaleAndKeepsSelection() {
        const combo = createTemporaryObject(comboComponent, testCase);
        combo.popup.open();
        tryCompare(combo.popup, "opened", true);
        compare(combo.popup.opacity, 1);
        compare(combo.popup.scale, 1);
        combo.popup.close();
        wait(30);
        combo.popup.open();
        tryCompare(combo.popup, "opened", true);
        compare(combo.popup.scale, 1);
        combo.currentIndex = 1;
        compare(combo.currentValue, "two");
        combo.popup.close();
        tryCompare(combo.popup, "visible", false);
        combo.popup.open();
        tryCompare(combo.popup, "opened", true);
        compare(combo.popup.scale, 1);
        compare(combo.currentIndex, 1);
        combo.popup.close();
        tryCompare(combo.popup, "visible", false);
    }
    function test_typingDoesNotWaitForFocusFeedback() {
        const field = createTemporaryObject(fieldComponent, testCase);
        field.focusInput();
        keyClick(Qt.Key_A);
        keyClick(Qt.Key_B);
        compare(field.text, "ab");
        field.text = "123";
        compare(field.text, "123");
    }
    function test_catalogTileIsImmediatelyVisible() {
        const tile = createTemporaryObject(tileComponent, testCase);
        compare(tile.opacity, 1);
        compare(tile.scale, 1);
    }
}
