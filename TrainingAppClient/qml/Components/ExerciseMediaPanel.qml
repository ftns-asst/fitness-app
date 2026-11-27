import QtQuick
import QtQuick.Controls
import QtMultimedia
import TrainingAppClient

Item {
    id: panel

    property var viewModel: null
    property bool pageActive: true
    property bool edgeToEdge: false
    property real fadeHeight: Math.min(100, height / 3)
    property real inlineControlsInset: 0
    property real backdropOffset: 0
    property real backdropOpacity: 1
    property string exerciseId: ""
    property real topInset: 0
    property real bottomInset: 0
    readonly property bool matchesExercise: viewModel !== null && (!exerciseId || viewModel.exerciseId === exerciseId)
    readonly property bool hasVideo: matchesExercise && viewModel.hasVideo === true
    readonly property string photoUrl: !hasVideo && matchesExercise ? (viewModel.photoUrl || "") : ""
    readonly property string videoUrl: hasVideo ? (viewModel.videoUrl || "") : ""
    readonly property double expiresAt: matchesExercise ? (viewModel.mediaExpiresAt || 0) : 0
    readonly property bool hasMedia: hasVideo || photoUrl !== ""
    readonly property bool videoLoading: hasVideo && viewModel.videoLoading === true
    readonly property bool foreground: pageActive && Qt.application.state === Qt.ApplicationActive
    readonly property var player: videoLoader.item ? videoLoader.item.player : null
    readonly property bool videoPlaying: player !== null && player.playbackState === MediaPlayer.PlayingState
    property bool playbackRequested: false
    property bool playbackFailed: false

    visible: hasMedia
    clip: !fullscreen.visible

    onExerciseIdChanged: clearPlayback()
    onHasVideoChanged: {
        if (!hasVideo)
            clearPlayback();
    }
    onHasMediaChanged: {
        if (!hasMedia)
            fullscreen.close();
    }
    onForegroundChanged: {
        if (!foreground)
            clearPlayback();
    }

    function clearPlayback() {
        playbackRequested = false;
        playbackFailed = false;
        fullscreen.close();
    }

    function showFullscreen() {
        if (hasMedia && foreground)
            fullscreen.open();
    }

    function togglePlayback() {
        if (videoPlaying) {
            player.pause();
        } else if (!player || playbackFailed) {
            playbackRequested = false;
            playbackFailed = false;
            playbackRequested = true;
            viewModel.loadVideo();
        } else {
            player.play();
        }
    }

    Timer {
        interval: running ? Math.max(1, panel.expiresAt - Date.now()) : 1
        running: panel.foreground && panel.expiresAt > 0 && !panel.viewModel.loading && !panel.videoPlaying
        onTriggered: panel.viewModel.refreshMedia()
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.surfaceMuted
        radius: panel.edgeToEdge ? 0 : Theme.radiusMd
    }

    Item {
        id: stage

        parent: fullscreen.visible ? fullscreenContent : panel
        anchors.fill: parent
        clip: true

        Item {
            width: parent.width
            height: parent.height
            y: fullscreen.visible ? 0 : panel.backdropOffset
            opacity: fullscreen.visible ? 1 : panel.backdropOpacity

            Image {
                id: photo
                objectName: "exercisePhoto"

                anchors.fill: parent
                asynchronous: true
                fillMode: fullscreen.visible ? Image.PreserveAspectFit : Image.Stretch
                opacity: status === Image.Ready ? 1 : 0
                source: panel.photoUrl
                visible: !panel.hasVideo

                Behavior on opacity {
                    NumberAnimation {
                        duration: Theme.pageTransitionDuration
                        easing.type: Easing.OutCubic
                    }
                }
            }

            Loader {
                id: videoLoader
                objectName: "exerciseVideoLoader"

                anchors.fill: parent
                active: panel.foreground && panel.playbackRequested && panel.videoUrl !== ""
                sourceComponent: Item {
                    property alias player: mediaPlayer

                    VideoOutput {
                        id: videoDisplay
                        anchors.fill: parent
                        fillMode: fullscreen.visible ? VideoOutput.PreserveAspectFit : VideoOutput.Stretch
                    }
                    MediaPlayer {
                        id: mediaPlayer
                        autoPlay: true
                        audioOutput: AudioOutput {}
                        source: panel.videoUrl
                        videoOutput: videoDisplay
                        onErrorOccurred: panel.playbackFailed = true
                    }
                }
            }
        }

        Label {
            anchors.centerIn: parent
            color: fullscreen.visible ? "white" : Theme.textSecondary
            font: Typography.detail
            horizontalAlignment: Text.AlignHCenter
            text: photo.status === Image.Error ? qsTr("Не удалось загрузить фото") : qsTr("Видео техники")
            visible: photo.status === Image.Error || (panel.hasVideo && !videoLoader.active)
            width: parent.width - 2 * Spacing.screenPadding
            wrapMode: Text.WordWrap
        }

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: Math.min(parent.height, panel.fadeHeight)
            visible: panel.edgeToEdge && !fullscreen.visible
            gradient: Gradient {
                GradientStop {
                    position: 0
                    color: "transparent"
                }
                GradientStop {
                    position: 0.52
                    color: Qt.rgba(Theme.background.r, Theme.background.g, Theme.background.b, 0.91)
                }
                GradientStop {
                    position: 1
                    color: Theme.background
                }
            }
        }

        Item {
            objectName: "exerciseMediaTapTarget"

            anchors.fill: parent
            anchors.bottomMargin: panel.hasVideo ? panel.inlineControlsInset + videoButton.height + 2 * Spacing.itemGap : 0
            enabled: panel.hasMedia && !fullscreen.visible
            activeFocusOnTab: enabled
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Открыть медиа на весь экран")
            Accessible.onPressAction: panel.showFullscreen()
            Keys.onReturnPressed: panel.showFullscreen()
            Keys.onSpacePressed: panel.showFullscreen()

            TapHandler {
                onTapped: panel.showFullscreen()
            }
        }

        AppButton {
            id: videoButton
            objectName: "exerciseVideoButton"

            anchors.bottom: parent.bottom
            anchors.bottomMargin: Spacing.itemGap + (fullscreen.visible ? panel.bottomInset : panel.inlineControlsInset)
            anchors.horizontalCenter: parent.horizontalCenter
            enabled: !panel.videoLoading
            labelFont: Typography.detail
            text: panel.videoLoading ? qsTr("Загружаем…") : (panel.playbackFailed || panel.viewModel?.videoDownloadFailed) ? qsTr("Повторить") : panel.videoPlaying ? qsTr("Пауза") : qsTr("Смотреть видео")
            variant: AppButton.Secondary
            visible: panel.hasVideo
            onClicked: panel.togglePlayback()
        }

        AppButton {
            objectName: "exerciseMediaCloseButton"

            anchors.right: parent.right
            anchors.rightMargin: Spacing.screenPadding
            anchors.top: parent.top
            anchors.topMargin: panel.topInset + Spacing.safeGap
            labelFont: Typography.detail
            text: qsTr("Закрыть")
            variant: AppButton.Secondary
            visible: fullscreen.visible
            onClicked: fullscreen.close()
        }
    }

    Popup {
        id: fullscreen
        objectName: "exerciseMediaFullscreen"

        parent: Overlay.overlay
        width: parent ? parent.width : 0
        height: parent ? parent.height : 0
        margins: 0
        padding: 0
        popupType: Popup.Item
        modal: true
        focus: true
        closePolicy: Popup.NoAutoClose

        background: Rectangle {
            color: "black"
        }
        contentItem: Item {
            id: fullscreenContent
        }

        enter: Transition {
            NumberAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: Theme.popupEnterDuration
            }
        }
        exit: Transition {
            NumberAnimation {
                property: "opacity"
                from: 1
                to: 0
                duration: Theme.popupExitDuration
            }
        }
    }

    BackHandler {
        enabled: fullscreen.visible
        priority: 30
        onBackPerformed: fullscreen.close()
    }
}
