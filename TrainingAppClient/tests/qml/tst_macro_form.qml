import QtQuick
import QtTest
import TrainingAppClient 1.0

TestCase {
    id: testCase
    name: "MacroForm"
    function cleanup() {
        model.hasWeight = true;
    }
    QtObject {
        id: model
        property bool hasWeight: true
        property real weightKg: 75
        property bool hasHeight: true
        property real heightCm: 170
        property bool hasBirthDate: true
        property string birthDate: "1995-06-10"
        property bool isFemale: true
        property string activityLevel: "light"
        function activityLevels() {
            return [];
        }
        property bool hasResult: false
        property string errorKey: ""
        property var submitted: []
        function load(user) {
            return true;
        }
        function calculate(user, weight, height, female, birth, activity, mode) {
            submitted = [weight, height, female, birth, activity, mode];
            return true;
        }
    }
    Component {
        id: form
        MacroCalculatorPage {
            viewModel: model
            width: 390
            height: 812
        }
    }
    function test_savedParametersPopulateForm() {
        const page = createTemporaryObject(form, testCase);
        verify(page !== null);
        compare(page.weightValue, 75);
        compare(page.heightValue, 170);
        compare(page.birthDate, "1995-06-10");
        compare(page.isFemaleSelected, true);
        compare(page.activityLevel, "light");
    }
    function test_calculationPreservesUserChangesWithoutSavedWeight() {
        model.hasWeight = false;
        const page = createTemporaryObject(form, testCase);
        page.weightValue = 82;
        page.heightValue = 182;
        page.isFemaleSelected = false;
        page.birthDate = "1990-01-01";
        page.activityLevel = "moderate";
        page.calculate();
        compare(model.submitted[0], 82);
        compare(model.submitted[1], 182);
        compare(model.submitted[2], false);
        compare(model.submitted[3], "1990-01-01");
        compare(model.submitted[4], "moderate");
        model.hasWeight = true;
    }
}
