pragma Singleton
import QtQuick

// Mock-backed presentation contract каталога упражнений. Поиск и фильтрация
// инкапсулированы здесь, чтобы UI не зависел от типа исходной модели.
QtObject {
    readonly property var exercises: MockCatalog.exercises
    readonly property var muscleGroups: {
        const groups = [];

        for (let i = 0; i < exercises.length; ++i) {
            const group = exercises[i].muscleGroup;

            if (groups.indexOf(group) < 0)
                groups.push(group);
        }
        return groups;
    }

    function filteredExercises(query, groupFilter) {
        const result = [];
        const queryText = String(query).trim().toLowerCase();

        for (let i = 0; i < exercises.length; ++i) {
            const item = exercises[i];
            const matchesGroup = groupFilter.length === 0 || item.muscleGroup === groupFilter;
            const matchesQuery = queryText.length === 0 || item.title.toLowerCase().indexOf(queryText) >= 0;

            if (matchesGroup && matchesQuery)
                result.push(item);
        }
        return result;
    }
}
