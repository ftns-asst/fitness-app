import QtQuick
import QtTest
import TrainingAppClient

TestCase {
    id: tests

    name: "ExerciseMedia"
    when: windowShown

    function test_videoIsLoadedOnlyAfterUserAction() {
        const page = createTemporaryObject(detail, tests.Window.window.contentItem);
        verify(page !== null);
        const photo = findChild(page, "exercisePhoto");
        const loader = findChild(page, "exerciseVideoLoader");
        const button = findChild(page, "exerciseVideoButton");
        verify(!button.visible);
        verify(!loader.active);

        page.viewModel.hasVideo = true;
        verify(button.visible);
        verify(!photo.visible);
        verify(!loader.active);
        compare(page.viewModel.videoRequests, 0);
        button.clicked();
        compare(page.viewModel.videoRequests, 1);
    }

    function test_heroFillsScreenAndFullscreenClosesOnBack() {
        const page = createTemporaryObject(detail, tests.Window.window.contentItem);
        verify(page !== null);
        page.viewModel.hasVideo = true;
        const panel = findChild(page, "exerciseDetailMedia");
        const media = findChild(page, "exerciseMediaTapTarget");
        const popup = findChild(panel, "exerciseMediaFullscreen");
        const photo = findChild(panel, "exercisePhoto");
        compare(panel.width, page.width);
        tryCompare(panel, "height", Math.round(page.height * 0.5));
        compare(photo.fillMode, Image.Stretch);

        mouseClick(media, media.width / 2, 100);
        tryCompare(popup, "opened", true);
        compare(popup.width, tests.Window.window.width);
        compare(popup.height, tests.Window.window.height);
        compare(photo.fillMode, Image.PreserveAspectFit);
        BackRouter.dispatchBack();
        tryCompare(popup, "visible", false);
        verify(panel.visible);
        compare(page.viewModel.videoRequests, 0);
    }

    Component {
        id: detail

        ExerciseDetailPage {
            width: 390
            height: 800
            viewModel: QtObject {
                property string name: "Press"
                property string errorKey: ""
                property string description: ""
                property string createdAt: ""
                property string photoUrl: ""
                property string videoUrl: ""
                property bool hasVideo: false
                property int videoRequests: 0
                property double mediaExpiresAt: Date.now() + 900000
                property bool hasExercise: true
                property bool loading: false
                property bool fromCache: false
                property bool isOwn: false
                property var muscleGroups: []
                property var equipments: []

                function loadVideo() {
                    videoRequests++;
                }
            }
        }
    }
}
