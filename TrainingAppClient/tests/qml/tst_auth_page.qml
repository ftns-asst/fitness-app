import QtQuick
import QtQuick.Window
import QtTest
import TrainingAppClient 1.0

TestCase {
    id: testCase

    property var page: null
    property var interactivePage: null
    property var interactiveWindow: null
    property var shell: null

    function cleanup() {
        MockApi.reset();
        if (interactiveWindow !== null)
            interactiveWindow.destroy();
        interactivePage = null;
        interactiveWindow = null;
        cancelRequestedSpy.clear();
        page = null;
        shell = null;
    }

    // Поиск элемента по objectName: страницы лежат внутри StackLayout
    // каркаса, поэтому прямой индекс в children нестабилен.
    function findByName(parent, name) {
        if (parent === null || parent === undefined)
            return null;

        const kids = parent.children;

        if (kids === undefined)
            return null;

        for (let i = 0; i < kids.length; ++i) {
            if (kids[i].objectName === name)
                return kids[i];

            const nested = findByName(kids[i], name);

            if (nested !== null)
                return nested;
        }
        return null;
    }
    function findProfile(host) {
        const profile = findByName(host, "profilePage");

        verify(profile !== null);
        return profile;
    }
    function init() {
        MockApi.reset();
        MockApi.responseDelayMs = 1;
        page = createTemporaryObject(authPageComponent, testCase);
        verify(page !== null);
    }

    // Части шапки «Профиля»: ids компонента снаружи не видны, поэтому ищем
    // элементы по objectName.
    function profilePart(host, name) {
        const part = findByName(host, name);

        verify(part !== null);
        return part;
    }
    function test_authPageBackClosesWhenClosable() {
        page.closable = true;

        let cancelled = 0;
        page.onCancelRequested.connect(function () {
            ++cancelled;
        });

        BackRouter.dispatchBack();
        compare(cancelled, 1);
    }
    function test_authPageBackIgnoredWhenNotClosable() {
        let cancelled = 0;
        page.onCancelRequested.connect(function () {
            ++cancelled;
        });

        BackRouter.dispatchBack();
        compare(cancelled, 0);
    }
    function clickNamed(host, name) {
        const item = profilePart(host, name);

        // objectName на Item-обёртке: клик уходит на дочерний AppButton.
        if (item.clicked !== undefined) {
            item.clicked();
            return;
        }

        verify(item.children.length > 0);
        item.children[0].clicked();
    }
    function test_authPageGuestModeIsClosable() {
        interactiveWindow = createTemporaryObject(authWindowComponent, testCase);
        verify(interactiveWindow !== null);
        interactivePage = interactiveWindow.authPage;
        verify(interactivePage !== null);
        interactivePage.closable = true;
        interactiveWindow.show();
        waitForRendering(interactivePage);

        const backButton = findByName(interactivePage, "backAuthButton");
        const backButtonContainer = findByName(interactivePage, "backAuthButtonContainer");

        verify(backButton !== null);
        verify(backButtonContainer !== null);
        compare(interactivePage.closable, true);
        compare(interactivePage.authState, "idle");
        compare(interactivePage.visible, true);
        tryCompare(backButtonContainer, "visible", true);
        tryCompare(backButton, "visible", true);

        mouseClick(backButton, backButton.width / 2, backButton.height / 2, Qt.LeftButton);
        compare(cancelRequestedSpy.count, 1);
    }
    function test_authPageNotClosableByDefault() {
        verify(!page.closable);
        verify(findByName(page, "backAuthButton") !== null);
    }
    function test_authPageWithoutSessionClosesOnSuccess() {
        page.closable = true;

        let authenticated = 0;
        page.onAuthenticated.connect(function () {
            ++authenticated;
        });

        page.loginEmail = "igor@example.com";
        page.loginPassword = "Strong1!";
        page.submitLogin();

        tryCompare(page, "authState", "authenticated", 2000);
        compare(authenticated, 1);
    }
    function test_guestHeaderReplacesAvatarWithLoginButton() {
        shell = createTemporaryObject(profileShellComponent, testCase);
        const profile = findProfile(shell);

        compare(profile.activeName, qsTr("Гость"));
        verify(profile.activeProfileDetails.length > 0);
        verify(profile.loginButtonVisible);
        verify(!profile.avatarVisible);
        verify(profilePart(profile, "profileHeader") !== null);
        verify(profilePart(profile, "profileLoginButton") !== null);
    }
    function test_loginFailureFromFailMode() {
        MockApi.failMode = "login:incorrect_password";
        MockApi.failOnce = true;
        page.loginEmail = "igor@example.com";
        page.loginPassword = "Strong1!";
        page.submitLogin();

        tryCompare(page, "authState", "error", 2000);
        compare(page.passwordError, ApiErrorText.textFor("incorrect_password"));
        verify(!MockSession.authenticated);
    }
    function test_loginSuccess() {
        page.loginEmail = "igor@example.com";
        page.loginPassword = "Strong1!";
        page.submitLogin();

        tryCompare(MockSession, "authenticated", true, 2000);
        compare(page.authState, "authenticated");
        compare(MockSession.user.email, "igor@example.com");
    }
    function test_loginValidation() {
        page.loginEmail = "bad-email";
        page.loginPassword = "Strong1!";
        page.submitLogin();

        compare(page.authState, "error");
        verify(page.emailError.length > 0);
        verify(!MockSession.authenticated);
    }
    function test_sessionExpirationOverridesFormState() {
        MockApi.startDemoSession();
        page.loginEmail = "bad-email";
        page.loginPassword = "Strong1!";
        page.submitLogin();
        compare(page.authState, "error");

        MockSession.expire();

        compare(page.authState, "sessionExpired");
    }
    function test_failedSignupDoesNotPersistLocalProfile() {
        MockApi.failMode = "signup:server_error";
        MockApi.failOnce = true;
        page.switchMode("signup");
        page.signupEmail = "failed@example.com";
        page.signupName = "Failed User";
        page.signupPassword = "Secure2@";
        page.signupAge = "31";
        page.signupHeight = "179";
        page.signupWeight = "74";
        page.signupGender = "male";
        page.signupGoal = "strength";
        page.emailFree = true;
        page.checkedEmail = "failed@example.com";

        page.submitRegistration();

        tryCompare(page, "authState", "error", 2000);
        compare(MockSession.localProfile, null);
        compare(page.pendingLocalProfile, null);
    }
    function test_logoutClearsSession() {
        MockApi.startDemoSession();
        verify(MockSession.authenticated);

        MockApi.logout();

        compare(MockSession.state, "anonymous");
        verify(!MockSession.authenticated);
        compare(MockSession.localProfile, null);
    }
    function test_logoutFromProfileRestoresLoginButton() {
        MockApi.startDemoSession();
        shell = createTemporaryObject(profileShellComponent, testCase);
        const profile = findProfile(shell);
        verify(MockSession.authenticated);
        verify(profile.avatarVisible);
        verify(!profile.loginButtonVisible);

        profile.sectionIndex = 2;
        clickNamed(profile, "profileLogoutButton");

        tryCompare(profile, "authenticated", false, 2000);
        verify(profile.loginButtonVisible);
        verify(!profile.avatarVisible);
        verify(!profile.logoutButtonVisible);
    }
    function test_profileLoginButtonOpensAuthSignal() {
        shell = createTemporaryObject(profileShellComponent, testCase);
        const profile = findProfile(shell);
        let requested = -1;
        shell.onAuthPageRequested.connect(function (originTab) {
            requested = originTab;
        });

        clickNamed(profile, "profileLoginButton");

        compare(requested, 3);
    }
    function test_profileShowsAvatarWhenAuthenticated() {
        MockApi.startDemoSession();
        shell = createTemporaryObject(profileShellComponent, testCase);
        const profile = findProfile(shell);

        verify(MockSession.authenticated);
        verify(profile.avatarVisible);
        verify(!profile.loginButtonVisible);
        verify(profile.logoutButtonVisible);
        verify(profilePart(profile, "profileAvatar") !== null);
        verify(profilePart(profile, "profileLogoutButton") !== null);
    }
    function test_profileShowsLoginButtonWhenAnonymous() {
        shell = createTemporaryObject(profileShellComponent, testCase);
        verify(shell !== null);
        verify(!MockSession.authenticated);

        const profile = findProfile(shell);

        verify(profile.loginButtonVisible);
        verify(!profile.avatarVisible);
        verify(!profile.logoutButtonVisible);
        verify(profilePart(profile, "profileLoginButton") !== null);
    }
    function test_signupSavesLocalProfile() {
        page.switchMode("signup");
        page.signupEmail = "new-user@example.com";
        page.signupName = "Игорь Морозов";
        page.signupPassword = "Secure2@";
        page.signupAge = "31";
        page.signupHeight = "179";
        page.signupWeight = "74";
        page.signupGender = "male";
        page.signupGoal = "strength";
        page.submitRegistration();

        tryCompare(MockSession, "authenticated", true, 3000);
        compare(page.authState, "authenticated");
        compare(MockSession.user.email, "new-user@example.com");
        compare(MockSession.profile, null);
        compare(MockSession.localProfile.age, 31);
        compare(MockSession.localProfile.heightCm, 179);
        compare(MockSession.localProfile.weightKg, 74);
        compare(MockSession.localProfile.goal, "strength");
    }
    function test_staleEmailCheckIsIgnored() {
        MockApi.responseDelayMs = 40;
        page.switchMode("signup");
        page.signupEmail = "first@example.com";
        page.checkEmailNow(false);
        page.signupEmail = "second@example.com";

        wait(100);
        compare(page.authState, "idle");
        verify(!page.emailFree);
        verify(!page.emailTaken);
    }
    function test_takenEmailOffersLogin() {
        page.switchMode("signup");
        page.signupEmail = "igor@example.com";
        page.checkEmailNow(false);

        tryCompare(page, "authState", "emailTaken", 2000);
        verify(page.emailTaken);
        verify(!page.emailFree);
        compare(page.emailError, ApiErrorText.textFor("email_taken"));
    }

    height: 812
    name: "AuthPage"
    when: windowShown
    width: 390

    SignalSpy {
        id: cancelRequestedSpy

        signalName: "cancelRequested"
        target: interactivePage
    }
    Component {
        id: authWindowComponent

        Window {
            property alias authPage: visualAuthPage

            height: 812
            visible: false
            width: 390

            AuthPage {
                id: visualAuthPage

                anchors.fill: parent
            }
        }
    }

    Component {
        id: authPageComponent

        AuthPage {
            height: 812
            width: 390
        }
    }
    Component {
        id: shellComponent

        AppShell {
            height: 812
            width: 390
        }
    }
    Component {
        id: profileShellComponent

        AppShell {
            height: 812
            initialTab: 3
            width: 390
        }
    }
}
