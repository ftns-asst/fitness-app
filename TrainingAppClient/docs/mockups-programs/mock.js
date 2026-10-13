// Общая логика HTML-макетов: переключение темы и ползунки.
// Тема вынесена из инлайновых блоков страниц, чтобы подключаться одним
// тегом; макет открывается с файла (file://), где модули недоступны.
function toggleTheme() {
    var el = document.documentElement;
    var n = el.getAttribute('data-theme') === 'dark' ? 'light' : 'dark';
    el.setAttribute('data-theme', n);
    try { localStorage.setItem('mock-theme', n); } catch (e) {}
}

try {
    var t = localStorage.getItem('mock-theme');
    if (t) document.documentElement.setAttribute('data-theme', t);
} catch (e) {}

// Ползунки: input[type=range] — источник истины, нарисованные заливка,
// маркер, крупное значение и подпись следуют за ним. Один обработчик
// обслуживает и срок недель, и целевое значение, и подходы с весом.
document.addEventListener('DOMContentLoaded', function () {
    document.querySelectorAll('.slider-input').forEach(function (input) {
        var box = input.closest('.slider');
        var fill = box.querySelector('.slider-fill');
        var thumb = box.querySelector('.slider-thumb');
        var value = box.querySelector('.slider-num');
        var unit = box.querySelector('.slider-unit');
        var hint = box.querySelector('.slider-hint');
        var min = Number(input.min);
        var max = Number(input.max);
        var decimals = Number(input.dataset.decimals || 0);

        function render() {
            var v = Number(input.value);
            var ratio = max === min ? 0 : (v - min) / (max - min);
            box.style.setProperty('--fill', (ratio * 100).toFixed(2) + '%');
            thumb.style.left = (ratio * 100).toFixed(2) + '%';
            value.textContent = v.toFixed(decimals).replace('.', ',');
            if (input.dataset.mirror) {
                document.querySelectorAll(input.dataset.mirror).forEach(function (m) { m.textContent = v; });
            }
            // Русское склонение: 1 неделя, 2–4 недели, 5+ недель.
            if (unit) unit.textContent = v === 1 ? 'неделя' : (v >= 2 && v <= 4 ? 'недели' : 'недель');
            // Дата окончания пересчитывается из срока и старта: срок без
            // даты окончания — абстракция, которую приходится считать в уме.
            if (hint && input.dataset.start) {
                var end = new Date(input.dataset.start);
                end.setDate(end.getDate() + (v * 7 - 1));
                hint.textContent = input.dataset.prefix + ' — ' + end.toLocaleDateString('ru-RU', { day: 'numeric', month: 'long', year: 'numeric' });
            }
        }

        input.addEventListener('input', render);
        render();
    });
});