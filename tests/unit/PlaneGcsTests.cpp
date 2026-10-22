// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Eduard Hart

#include <GCS.h>

#include <cmath>
#include <iostream>

namespace {

constexpr double tolerance = 1.0e-7;

bool close(double first, double second) { return std::abs(first - second) <= tolerance; }

bool lineLengthAndHorizontalConstraintSolve() {
    double firstX = 0.0;
    double firstY = 0.0;
    double secondX = 3.0;
    double secondY = 4.0;
    double length = 10.0;
    GCS::Point first{&firstX, &firstY};
    GCS::Point second{&secondX, &secondY};
    GCS::Line line;
    line.p1 = first;
    line.p2 = second;
    GCS::System system;
    static_cast<void>(system.addConstraintP2PDistance(first, second, &length, 1));
    static_cast<void>(system.addConstraintHorizontal(line, 2));
    GCS::VEC_pD unknowns{&firstX, &firstY, &secondX, &secondY};
    const int status = system.solve(unknowns);
    if (status == GCS::Success)
        system.applySolution();
    if (status != GCS::Success || system.dofsNumber() != 2) {
        std::cerr << "status=" << status << " dofs=" << system.dofsNumber() << " line=(" << firstX
                  << ',' << firstY << ")-(" << secondX << ',' << secondY << ")\n";
        return false;
    }
    const double solvedLength = std::hypot(secondX - firstX, secondY - firstY);
    if (!close(firstY, secondY) || !close(solvedLength, 10.0)) {
        std::cerr << "solved line=(" << firstX << ',' << firstY << ")-(" << secondX << ','
                  << secondY << ") length=" << solvedLength << '\n';
        return false;
    }
    return true;
}

bool circleRadiusConstraintSolves() {
    double centerX = 4.0;
    double centerY = 5.0;
    double radius = 2.0;
    double requestedRadius = 7.5;
    GCS::Circle circle;
    circle.center = {&centerX, &centerY};
    circle.rad = &radius;
    GCS::System system;
    static_cast<void>(system.addConstraintCircleRadius(circle, &requestedRadius, 1));
    GCS::VEC_pD unknowns{&centerX, &centerY, &radius};
    const int status = system.solve(unknowns);
    if (status == GCS::Success)
        system.applySolution();
    return status == GCS::Success && system.dofsNumber() == 2 && close(radius, requestedRadius);
}

bool geometricRelationsReportExpectedDegreesOfFreedom() {
    double firstX = 0.0;
    double firstY = 0.0;
    double secondX = 3.0;
    double secondY = 1.0;
    double thirdX = 5.0;
    double thirdY = 2.0;
    double fourthX = 7.0;
    double fourthY = 5.0;
    GCS::Point first{&firstX, &firstY};
    GCS::Point second{&secondX, &secondY};
    GCS::Point third{&thirdX, &thirdY};
    GCS::Point fourth{&fourthX, &fourthY};
    GCS::Line firstLine;
    firstLine.p1 = first;
    firstLine.p2 = second;
    GCS::Line secondLine;
    secondLine.p1 = third;
    secondLine.p2 = fourth;
    GCS::System system;
    static_cast<void>(system.addConstraintP2PCoincident(second, third, 1));
    static_cast<void>(system.addConstraintPerpendicular(firstLine, secondLine, 2));
    GCS::VEC_pD unknowns{&firstX, &firstY, &secondX, &secondY,
                         &thirdX, &thirdY, &fourthX, &fourthY};
    const int status = system.solve(unknowns);
    if (status == GCS::Success)
        system.applySolution();
    if (status != GCS::Success || system.dofsNumber() != 5)
        return false;
    const double dot =
        (secondX - firstX) * (fourthX - thirdX) + (secondY - firstY) * (fourthY - thirdY);
    return close(secondX, thirdX) && close(secondY, thirdY) && close(dot, 0.0);
}

bool contradictoryConstraintsAreDiagnosed() {
    double firstX = 0.0;
    double firstY = 0.0;
    double secondX = 3.0;
    double secondY = 4.0;
    double length = 5.0;
    GCS::Point first{&firstX, &firstY};
    GCS::Point second{&secondX, &secondY};
    GCS::Line line;
    line.p1 = first;
    line.p2 = second;
    GCS::System system;
    static_cast<void>(system.addConstraintP2PDistance(first, second, &length, 1));
    static_cast<void>(system.addConstraintHorizontal(line, 2));
    static_cast<void>(system.addConstraintVertical(line, 3));
    GCS::VEC_pD unknowns{&firstX, &firstY, &secondX, &secondY};
    system.declareUnknowns(unknowns);
    static_cast<void>(system.diagnose());
    GCS::VEC_I conflicting;
    system.getConflicting(conflicting);
    return !conflicting.empty();
}

} // namespace

int main() {
    if (!lineLengthAndHorizontalConstraintSolve()) {
        std::cerr << "PlaneGCS line dimension solve failed\n";
        return 1;
    }
    if (!circleRadiusConstraintSolves()) {
        std::cerr << "PlaneGCS circle radius solve failed\n";
        return 1;
    }
    if (!geometricRelationsReportExpectedDegreesOfFreedom()) {
        std::cerr << "PlaneGCS relation or degree-of-freedom solve failed\n";
        return 1;
    }
    if (!contradictoryConstraintsAreDiagnosed()) {
        std::cerr << "PlaneGCS did not diagnose contradictory constraints\n";
        return 1;
    }
    return 0;
}
