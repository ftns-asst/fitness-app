import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

Column {
    id: control

    property string label: ""
    property string caption: ""
    property string unit: ""
    property real from: 0
    property real to: 1000
    property real step: 1
    property int decimals: 0
    property bool showDivider: true
    property alias text: input.text
    readonly property bool ruler: decimals > 0
    readonly property real numberValue: parseNumber(text)
    readonly property bool valid: inRange(numberValue)
    readonly property real tickSpacing: ruler ? 18 : 44
    readonly property int lastIndex: Math.floor((to - from) / step)
    property bool updatingFromScale: false
    property bool positioningScale: false
    readonly property bool gripping: scaleTap.pressed || scaleView.dragging
    property real gripProgress: gripping ? 1 : 0

    bottomPadding: Spacing.itemGap
    leftPadding: Spacing.cardPadding
    rightPadding: Spacing.cardPadding
    topPadding: Spacing.itemGap

    Behavior on gripProgress {
        NumberAnimation {
            duration: Theme.pressFeedbackDuration
            easing.type: Easing.OutCubic
        }
    }

    function parseNumber(value) {
        return value.trim().length > 0 ? Number(value.replace(",", ".")) : NaN;
    }

    function inRange(value) {
        return Number.isFinite(value) && value >= from && value <= to && (ruler || Number.isInteger(value));
    }

    function writeNumber(value) {
        input.text = String(Number(value.toFixed(decimals))).replace(".", Qt.locale().decimalPoint);
    }

    function setValue(value, animate = false) {
        const bounded = Math.min(to, Math.max(from, Number(value)));
        if (!Number.isFinite(bounded))
            return;
        positioningScale = true;
        snapMotion.stop();
        scaleView.cancelFlick();
        writeNumber(bounded);
        const position = (Number(bounded.toFixed(decimals)) - from) / step * tickSpacing;
        if (animate && Math.abs(scaleView.contentX - position) > 1) {
            snapMotion.to = position;
            snapMotion.start();
        } else {
            scaleView.contentX = position;
        }
        positioningScale = false;
    }

    function selectAt(position, snap) {
        const index = Math.max(0, Math.min(lastIndex, Math.round(position / tickSpacing)));
        const selected = from + index * step;
        if (snap) {
            setValue(selected, true);
        } else {
            updatingFromScale = true;
            writeNumber(selected);
            updatingFromScale = false;
        }
    }

    RowLayout {
        width: control.width - control.leftPadding - control.rightPadding

        Column {
            Layout.fillWidth: true
            Layout.minimumWidth: 0

            Label {
                color: Theme.textSecondary
                font: Typography.captionStrong
                text: control.label
                width: parent.width
                wrapMode: Text.WordWrap
            }
            Label {
                color: Theme.textMuted
                font: Typography.caption
                text: control.caption
                visible: text.length > 0
                width: parent.width
                wrapMode: Text.WordWrap
            }
        }
        AppInput {
            id: input

            objectName: "valueInput"
            Accessible.name: control.label
            Layout.minimumWidth: 96
            Layout.preferredWidth: 116
            Layout.preferredHeight: Theme.touchMin
            color: control.valid ? Theme.textPrimary : Theme.negative
            font: Typography.workoutTitle
            horizontalAlignment: Text.AlignRight
            inputMethodHints: control.ruler ? Qt.ImhFormattedNumbersOnly : Qt.ImhDigitsOnly
            leftPadding: 4
            rightPadding: 4
            selectByMouse: true
            text: "0"

            background: Rectangle {
                color: input.activeFocus ? Theme.surfaceMuted : "transparent"
                border.color: control.valid ? Theme.accent : Theme.negative
                border.width: input.activeFocus ? 1 : 0
                radius: Theme.radiusSm
            }
            validator: RegularExpressionValidator {
                regularExpression: control.ruler ? /^\d{0,4}([.,]\d{0,2})?$/ : /^\d{0,5}$/
            }
            onActiveFocusChanged: {
                if (activeFocus) {
                    snapMotion.stop();
                    scaleView.cancelFlick();
                    selectAll();
                }
            }
            onTextChanged: {
                const typed = control.parseNumber(input.text);
                if (control.inRange(typed) && !control.positioningScale && !control.updatingFromScale) {
                    snapMotion.stop();
                    scaleView.contentX = (typed - control.from) / control.step * control.tickSpacing;
                }
            }
        }
        Label {
            color: Theme.textSecondary
            font: Typography.captionStrong
            text: control.unit
            visible: text.length > 0
        }
    }

    Item {
        width: control.width - control.leftPadding - control.rightPadding
        height: control.ruler ? 53 : 48

        NumberAnimation {
            id: snapMotion

            target: scaleView
            property: "contentX"
            duration: Theme.selectionDuration
            easing.type: Easing.OutCubic
        }
        Flickable {
            id: scaleView

            objectName: "workoutValueScale"
            anchors.fill: parent
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            contentWidth: width + control.lastIndex * control.tickSpacing
            contentHeight: height
            flickableDirection: Flickable.HorizontalFlick
            flickDeceleration: Theme.flickDeceleration
            maximumFlickVelocity: Theme.flickMaxVelocity
            pixelAligned: true

            readonly property int firstTick: Math.max(0, Math.floor((contentX - width / 2) / control.tickSpacing))
            readonly property int visibleTicks: Math.max(0, Math.min(control.lastIndex - firstTick + 1, Math.ceil(width / control.tickSpacing) + 2))

            onDraggingChanged: {
                if (dragging)
                    snapMotion.stop();
            }
            onContentXChanged: {
                if (moving && !control.positioningScale)
                    control.selectAt(contentX, false);
            }
            onMovementEnded: {
                if (!control.positioningScale)
                    control.selectAt(contentX, true);
            }

            Repeater {
                model: scaleView.visibleTicks

                delegate: Item {
                    id: tick

                    required property int index
                    readonly property int tickIndex: scaleView.firstTick + index
                    readonly property real value: control.from + tickIndex * control.step
                    readonly property bool major: tickIndex % 4 === 0
                    readonly property bool selected: value === control.numberValue

                    x: scaleView.width / 2 + tickIndex * control.tickSpacing - width / 2
                    width: control.tickSpacing
                    height: scaleView.height

                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: 5
                        width: tick.major ? 2 : 1
                        height: tick.major ? 20 : 11
                        color: tick.major ? Theme.textSecondary : Theme.border
                        visible: control.ruler
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: control.ruler ? 30 : (parent.height - height) / 2
                        color: !control.ruler && tick.selected ? Theme.accentInk : Theme.textSecondary
                        font: control.ruler ? Typography.caption : tick.selected ? Typography.sectionTitle : Typography.detail
                        fontSizeMode: Text.HorizontalFit
                        horizontalAlignment: Text.AlignHCenter
                        minimumPixelSize: 9
                        text: String(tick.value)
                        visible: !control.ruler || (tick.major && tick.x >= scaleView.contentX - tick.width / 2 + width / 2 && tick.x <= scaleView.contentX + scaleView.width - tick.width / 2 - width / 2)
                        width: control.ruler ? implicitWidth : tick.width - 4
                    }
                }
            }
            TapHandler {
                id: scaleTap

                gesturePolicy: TapHandler.DragThreshold

                onPressedChanged: {
                    if (pressed)
                        snapMotion.stop();
                }

                onTapped: point => {
                    const viewport = scaleView.mapFromItem(null, point.scenePosition.x, point.scenePosition.y);
                    control.selectAt(scaleView.contentX + viewport.x - scaleView.width / 2, true);
                }
            }
        }

        Rectangle {
            objectName: "workoutScaleMarker"
            anchors.horizontalCenter: parent.horizontalCenter
            y: control.ruler ? 15.5 - height / 2 : (parent.height - height) / 2
            width: control.ruler ? 2 + control.gripProgress : control.tickSpacing - 2 + 2 * control.gripProgress
            height: control.ruler ? 23 + 8 * control.gripProgress : 44 + 2 * control.gripProgress
            radius: control.ruler ? width / 2 : Theme.radiusMd
            color: control.ruler ? Theme.accent : "transparent"
            border.color: Theme.accent
            border.width: control.ruler ? 0 : 1
        }
    }
    Rectangle {
        color: Theme.borderSubtle
        height: 1
        visible: control.showDivider
        width: control.width - control.leftPadding - control.rightPadding
    }
}
