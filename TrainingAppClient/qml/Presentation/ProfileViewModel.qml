pragma Singleton
import QtQuick

// QML-контракт серверного профиля: запросы и кэш принадлежат ProfileBackend.
QtObject {
    id: viewModel
    property var backend: typeof ProfileBackend !== "undefined" ? ProfileBackend : null
    readonly property bool canEdit: backend !== null
    readonly property var profile: backend !== null ? backend.profile : ({})
    readonly property bool loading: backend !== null && backend.loading
    readonly property bool saving: backend !== null && backend.saving
    readonly property bool offline: backend !== null && backend.offline
    readonly property bool fromCache: backend !== null && backend.fromCache
    readonly property bool pendingSync: backend !== null && backend.pendingSync
    readonly property bool syncing: backend !== null && backend.syncing
    readonly property bool syncBlocked: backend !== null && backend.syncBlocked
    readonly property string syncErrorText: backend !== null ? ApiErrorText.textFor(backend.syncErrorKey) : ""
    readonly property string errorText: backend !== null ? ApiErrorText.textFor(backend.errorKey) : ""
    readonly property var profileParams: [
        {
            label: qsTr("Возраст"),
            value: profile.age !== undefined ? qsTr("%1 лет").arg(profile.age) : "—"
        },
        {
            label: qsTr("Пол"),
            value: profile.gender === "male" ? qsTr("Мужской") : profile.gender === "female" ? qsTr("Женский") : "—"
        },
        {
            label: qsTr("Рост"),
            value: profile.heightCm !== undefined ? qsTr("%1 см").arg(profile.heightCm) : "—"
        },
        {
            label: qsTr("Вес"),
            value: profile.weightKg !== undefined ? qsTr("%1 кг").arg(profile.weightKg) : "—"
        }
    ]
    property Connections backendEvents: Connections {
        target: viewModel.backend
        function onProfileSaved() {
            viewModel.profileSaved();
        }
        function onChanged() {
            if (AuthViewModel.authenticated && viewModel.backend.userId === String(AuthViewModel.user.id))
                AuthViewModel.updateProfile(viewModel.profile);
        }
    }
    property Connections accountEvents: Connections {
        target: AuthViewModel
        function onUserChanged() {
            viewModel.reload(AuthViewModel.authenticated ? String(AuthViewModel.user.id) : "");
        }
        function onAuthenticatedChanged() {
            viewModel.reload(AuthViewModel.authenticated ? String(AuthViewModel.user.id) : "");
        }
    }
    signal profileSaved
    function reload(userId) {
        if (backend !== null)
            backend.reload(userId);
    }
    function saveProfile(changes) {
        return backend !== null && backend.saveProfile(changes);
    }
    function retrySync() {
        if (backend !== null)
            backend.retrySync();
    }
}
