/**
 * @file main.cpp
 * @brief Основная точка входа программы hatch_generator.
 *
 * выполняется с помощью (пример) ./hatch_generator --angle 45 --step 1.
 * 
 * Программа генерирует линии (штриховку) под заданным углом и шагом,
 * выполняет обрезку линий алгоритмом Коэна–Сазерленда и сохраняет результат в SVG-файл.
 *
 * Поддерживаемые параметры:
 * - `--angle <число>` - угол наклона линий в градусах.
 * - `--step <число>` - расстояние между линиями.
 * 
 * Результат сохраняется в файл `hatch.svg` в папке сборки.
 */

#include <iostream>
#include <vector>
#include <cmath>
#include <numbers>
#include <fstream>
#include <string>

 /**
  * @brief Точка в 2D пространстве.
  */
struct Point_2 {
    /**
     * @brief Координата X.
     */
    double x;

    /**
     * @brief Координата Y.
     */
    double y;
};

/**
 * @brief Линия, представленная двумя точками (начало и конец).
 */
struct Line_2 {
    /**
     * @brief Начальная точка линии.
     */
    Point_2 start;

    /**
     * @brief Конечная точка линии.
     */
    Point_2 end;
};

/// Контур - список точек.
using Contour = std::vector<Point_2>;
/// Коллекция контуров.
using Contours = std::vector<Contour>;
/// Коллекция линий.
using Lines = std::vector<Line_2>;

/**
 * @brief Конвертирует угол из градусов в радианы.
 * @param degrees Угол в градусах.
 * @return Угол в радианах.
 */
double degreesToRadians(double degrees) { return degrees * std::numbers::pi / 180.0; }

/**
 * @enum OutCode
 * @brief Коды положения точки относительно прямоугольника
 * (используются в алгоритме Коэна–Сазерленда).
 *
 * INSIDE – внутри
 * LEFT / RIGHT – вне по X
 * BOTTOM / TOP – вне по Y
 */
enum OutCode { INSIDE = 0, LEFT = 1, RIGHT = 2, BOTTOM = 4, TOP = 8 };

/**
 * @brief Вычисляет OutCode для точки относительно прямоугольника.
 * @param x Координата X точки.
 * @param y Координата Y точки.
 * @param bottomLeft Нижняя левая точка прямоугольника.
 * @param topRight Верхняя правая точка прямоугольника.
 * @return Код положения.
 */
int computeOutCode(double x, double y, const Point_2& bottomLeft, const Point_2& topRight) {
    int code = INSIDE;
    if (x < bottomLeft.x) code |= LEFT;
    else if (x > topRight.x) code |= RIGHT;

    if (y < bottomLeft.y) code |= BOTTOM;
    else if (y > topRight.y) code |= TOP;

    return code;
}

/**
 * @brief Обрезает линию в пределах прямоугольника по алгоритму Коэна–Сазерленда.
 *
 * @param line Линия для обрезки. На выходе содержит усечённую версию.
 * @param bottomLeft Нижняя левая точка ограничивающего прямоугольника.
 * @param topRight Верхняя правая точка прямоугольника.
 * @return true, если линия пересекает прямоугольник и была обрезана;
 *         false, если линия полностью вне прямоугольника.
 */
bool clipLine(Line_2& line, const Point_2& bottomLeft, const Point_2& topRight) {
    double x0 = line.start.x, y0 = line.start.y;
    double x1 = line.end.x, y1 = line.end.y;

    int outcode0 = computeOutCode(x0, y0, bottomLeft, topRight);
    int outcode1 = computeOutCode(x1, y1, bottomLeft, topRight);
    bool accept = false;

    while (true) {
        if (!(outcode0 | outcode1)) {   // Оба внутри
            accept = true;
            break;
        }
        else if (outcode0 & outcode1) { // Полностью вне
            break;
        }
        else {
            double x, y;
            int outcodeOut = outcode0 ? outcode0 : outcode1;

            if (outcodeOut & TOP) {
                x = x0 + (x1 - x0) * (topRight.y - y0) / (y1 - y0);
                y = topRight.y;
            }
            else if (outcodeOut & BOTTOM) {
                x = x0 + (x1 - x0) * (bottomLeft.y - y0) / (y1 - y0);
                y = bottomLeft.y;
            }
            else if (outcodeOut & RIGHT) {
                y = y0 + (y1 - y0) * (topRight.x - x0) / (x1 - x0);
                x = topRight.x;
            }
            else { // LEFT
                y = y0 + (y1 - y0) * (bottomLeft.x - x0) / (x1 - x0);
                x = bottomLeft.x;
            }

            if (outcodeOut == outcode0) {
                x0 = x; y0 = y;
                outcode0 = computeOutCode(x0, y0, bottomLeft, topRight);
            }
            else {
                x1 = x; y1 = y;
                outcode1 = computeOutCode(x1, y1, bottomLeft, topRight);
            }
        }
    }

    if (accept) {
        line.start = { x0, y0 };
        line.end = { x1, y1 };
        return true;
    }
    return false;
}

/**
 * @brief Точка входа программы.
 *
 * Разбирает аргументы командной строки, генерирует набор линий под углом,
 * выполняет обрезку по прямоугольнику и записывает результат в SVG.
 *
 * @param argc Количество аргументов.
 * @param argv Массив аргументов.
 * @return Код выхода.
 */
int main(int argc, char* argv[]) {
    Contours contoursPoints;
    Lines hatchLines;

    // --- Пример исходного прямоугольника ---
    contoursPoints.push_back({ {0,0}, {20,0}, {20,10}, {0,10} });
    Point_2 bottomLeft = contoursPoints[0][0];
    Point_2 topRight = contoursPoints[0][2];

    // --- Разбор аргументов ---
    double angleDegrees = 45;
    double step = 1;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--angle" && i + 1 < argc) angleDegrees = std::stod(argv[++i]);
        else if (arg == "--step" && i + 1 < argc) step = std::stod(argv[++i]);
    }
    if (step <= 0) {
        std::cerr << "Error: step must be greater than zero\n";
        return 1;
    }

    angleDegrees = std::fmod(angleDegrees, 360.0);
    if (angleDegrees < 0) angleDegrees += 360.0;

    double angleRadians = degreesToRadians(angleDegrees);

    double width = topRight.x - bottomLeft.x;
    double height = topRight.y - bottomLeft.y;
    double diagonal = std::sqrt(width * width + height * height);

    Point_2 center{
        (bottomLeft.x + topRight.x) / 2,
        (bottomLeft.y + topRight.y) / 2
    };

    // --- Генерация линий ---
    if (angleDegrees == 0 || angleDegrees == 360) {
        // Горизонтальные линии сверху вниз
        for (double y = bottomLeft.y; y <= topRight.y; y += step) {
            hatchLines.push_back({ {bottomLeft.x, y}, {topRight.x, y} });
        }
    }
    else if (angleDegrees == 180 || angleDegrees == -180) {
        // Горизонтальные линии снизу вверх
        for (double y = topRight.y; y >= bottomLeft.y; y -= step) {
            hatchLines.push_back({ {bottomLeft.x, y}, {topRight.x, y} });
        }
    }
    else if (angleDegrees == 90 || angleDegrees == -90 || angleDegrees == 270 || angleDegrees == -270) {
        // Вертикальные линии слева направо
        for (double x = bottomLeft.x; x <= topRight.x; x += step) {
            hatchLines.push_back({ {x, bottomLeft.y}, {x, topRight.y} });
        }
    }
    else {
        for (double offset = -diagonal / 2; offset <= diagonal / 2; offset += step) {
            Point_2 dir{ std::cos(angleRadians), std::sin(angleRadians) };
            Point_2 perp{ -dir.y, dir.x };

            Line_2 line;
            line.start = { center.x + perp.x * offset - dir.x * diagonal / 2,
                           center.y + perp.y * offset - dir.y * diagonal / 2 };

            line.end = { center.x + perp.x * offset + dir.x * diagonal / 2,
                           center.y + perp.y * offset + dir.y * diagonal / 2 };

            if (clipLine(line, bottomLeft, topRight))
                hatchLines.push_back(line);
        }
    }

    // --- Лог вывод ---
    int lineNumber = 1;
    for (const auto& line : hatchLines) {
        std::cout << "Line " << lineNumber++
            << ": (" << line.start.x << "," << line.start.y
            << ") -> (" << line.end.x << "," << line.end.y << ")\n";
    }

    // --- Генерация SVG ---
    std::ofstream svg("hatch.svg");
    double scale = 10.0;

    svg << "<svg xmlns='http://www.w3.org/2000/svg' width='300' height='200'>\n";

    for (const auto& line : hatchLines) {
        svg << "<line x1='" << line.start.x * scale
            << "' y1='" << line.start.y * scale
            << "' x2='" << line.end.x * scale
            << "' y2='" << line.end.y * scale
            << "' stroke='black' stroke-width='0.5'/>\n";
    }

    // Рисуем прямоугольник
    Contour rect = contoursPoints[0];
    for (size_t i = 0; i < rect.size(); ++i) {
        Point_2 p1 = rect[i];
        Point_2 p2 = rect[(i + 1) % rect.size()];

        svg << "<line x1='" << p1.x * scale
            << "' y1='" << p1.y * scale
            << "' x2='" << p2.x * scale
            << "' y2='" << p2.y * scale
            << "' stroke='red' stroke-width='1'/>\n";
    }

    svg << "</svg>";
    svg.close();

    std::cout << "SVG file generated: hatch.svg\n";
    system("pause");
    return 0;
}
