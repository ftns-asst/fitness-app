import QtQuick
import QtTest
import TrainingAppClient 1.0

// Быстрые контролы: плитка правила (одно касание — отметка) и степпер
// подходов/веса (шаг, границы, сигнал edited).
TestCase {
    id: testCase

    name: "QuickControls"
    when: windowShown
    width: 400
    height: 300

    SignalSpy {
        id: toggledSpy

        signalName: "toggled"
    }
    SignalSpy {
        id: openedSpy

        signalName: "opened"
    }
    SignalSpy {
        id: editedSpy

        signalName: "edited"
    }

    Component {
        id: tileComponent

        RuleTile {
            height: 96
            title: "Сон 7–9 часов"
            width: 160
        }
    }
    Component {
        id: stepperComponent

        CompactStepper {
            from: 1
            step: 1
            to: 5
            value: 5
            width: 200
        }
    }

    // Тап через MouseArea плитки: элемент не в окне, поэтому симуляция мыши ненадёжна.
    function tap(tile) {
        for (let i = 0; i < tile.children.length; ++i) {
            if (tile.children[i].objectName === "tileArea") {
                tile.children[i].clicked(null);
                return;
            }
        }

        fail("у плитки нет области касания");
    }
    function test_tileTapTogglesWhenCheckable() {
        const tile = createTemporaryObject(tileComponent, testCase);

        toggledSpy.target = tile;
        toggledSpy.clear();
        testCase.tap(tile);
        compare(toggledSpy.count, 1, "тап по плитке не отправил toggled");
        verify(tile.done === false);

        tile.checkinState = "done";
        verify(tile.done, "состояние done не распознано");
    }
    // Плитка числового правила: текст на месте, прогресс считается от цели, превышение «не выше» подсвечено.
    function test_numericTileProgressAndText() {
        const tile = createTemporaryObject(tileComponent, testCase, {
            goalMin: 140,
            numeric: true,
            unitText: "г",
            value: 70
        });

        compare(tile.title, "Сон 7–9 часов", "название не дошло до плитки");
        compare(tile.progress, 0.5);
        compare(tile.valueText(), "70 / 140 г");
        verify(tile.exceeded === false);

        tile.value = 200;
        compare(tile.progress, 1, "полоса вышла за 100 %");
    }
    function test_maxGoalExceededTurnsRed() {
        const tile = createTemporaryObject(tileComponent, testCase, {
            goalMax: 2200,
            numeric: true,
            unitText: "ккал",
            value: 2500
        });

        verify(tile.exceeded, "превышение «не выше» не распознано");
        compare(tile.accentColor, Theme.negative);
    }
    function test_numericTapOpensEditor() {
        const tile = createTemporaryObject(tileComponent, testCase, {
            goalMin: 140,
            numeric: true
        });
        const spy = createTemporaryObject(spyComponent, testCase, {
            target: tile
        });

        testCase.tap(tile);
        compare(spy.count, 1, "тап по числовой плитке не открыл ввод");
    }
    Component {
        id: spyComponent

        SignalSpy {
            signalName: "edit"
        }
    }
    function test_adviceTileOnlyOpens() {
        const tile = createTemporaryObject(tileComponent, testCase, {
            checkable: false
        });

        toggledSpy.target = tile;
        openedSpy.target = tile;
        toggledSpy.clear();
        openedSpy.clear();
        testCase.tap(tile);
        compare(toggledSpy.count, 0, "у совета не должно быть отметки");
        compare(openedSpy.count, 1);
    }
    function test_stepperClampsAtBounds() {
        const stepper = createTemporaryObject(stepperComponent, testCase);

        editedSpy.target = stepper;
        editedSpy.clear();

        // Верхняя граница: значение 5 из 5 не растёт.
        stepper.change(1);
        compare(editedSpy.count, 0, "степпер вышел за верхнюю границу");

        stepper.change(-1);
        compare(editedSpy.count, 1);
        compare(editedSpy.signalArguments[0][0], 4);

        stepper.value = 1;
        editedSpy.clear();
        stepper.change(-1);
        compare(editedSpy.count, 0, "степпер вышел за нижнюю границу");
    }
}
