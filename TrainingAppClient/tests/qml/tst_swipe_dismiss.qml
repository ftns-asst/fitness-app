import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtTest
import TrainingAppClient 1.0

TestCase {
    id: testCase
    name: "SwipeDismiss"
    when: windowShown

    Component {
        id: fixture
        Window {
            width: 390
            height: 812
            property alias popup: popup
            property alias handle: grip
            property alias input: input
            SwipePopup {
                id: popup
                width: 340
                height: 400
                restingY: 150
                dragHandle: grip
                modal: true
                contentItem: ColumnLayout {
                    SwipeDismissHandle {
                        id: grip
                        Layout.fillWidth: true
                        onDismissed: popup.close()
                    }
                    TextField {
                        id: input
                        Layout.fillWidth: true
                    }
                    Flickable {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        contentHeight: 1000
                        contentWidth: width
                        clip: true
                        Rectangle {
                            width: parent.width
                            height: 1000
                            color: "lightgray"
                        }
                    }
                }
            }
        }
    }
    function openWindow() {
        const window = createTemporaryObject(fixture, testCase);
        window.show();
        window.popup.open();
        tryCompare(window.popup, "opened", true);
        return window;
    }
    function dragHandle(window, dx, dy) {
        const start = window.handle.mapToItem(window.contentItem, window.handle.width / 2, window.handle.height / 2);
        mousePress(window.contentItem, start.x, start.y);
        wait(150);
        mouseMove(window.contentItem, start.x + dx / 2, start.y + dy / 2, 30);
        mouseMove(window.contentItem, start.x + dx, start.y + dy, 30);
        if (dy > 30)
            verify(window.popup.y > 150, "окно должно следовать за пальцем");
        mouseRelease(window.contentItem, start.x + dx, start.y + dy);
    }
    function test_downwardDragClosesAndReopeningKeepsInput() {
        const window = openWindow();
        window.input.text = "Черновик";
        dragHandle(window, 0, 130);
        tryCompare(window.popup, "visible", false);
        window.popup.open();
        tryCompare(window.popup, "opened", true);
        compare(window.handle.offset, 0);
        compare(window.popup.y, 150);
        compare(window.input.text, "Черновик");
    }
    function test_shortDragReturnsToOriginalPosition() {
        const window = openWindow();
        dragHandle(window, 0, 30);
        verify(window.popup.visible);
        tryCompare(window.handle, "offset", 0);
        compare(window.popup.y, 150);
    }
    function test_horizontalAndUpwardDragsDoNotClose() {
        const window = openWindow();
        dragHandle(window, 100, 0);
        verify(window.popup.visible);
        compare(window.handle.offset, 0);
        dragHandle(window, 0, -100);
        verify(window.popup.visible);
        compare(window.handle.offset, 0);
    }
}
