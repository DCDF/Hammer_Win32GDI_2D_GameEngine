#include <iostream>

#include "tree/quadtree/QuadTree.h"

int main()
{
    // 创建一个覆盖区域从(0,0)到(100,100)，节点容量为4的四叉树
    QuadTree qt(Rect(0, 0, 100, 100), 4);

    // 插入一些点
    std::vector<Point> pointsToInsert = {
        Point(10, 10), Point(20, 20), Point(30, 30), Point(40, 40),
        Point(50, 50), Point(60, 60), Point(70, 70), Point(80, 80)};

    std::cout << "Inserting points:" << std::endl;
    for (auto &p : pointsToInsert)
    {
        if (qt.insert(p))
        {
            std::cout << "Point (" << p.x << ", " << p.y << ") inserted." << std::endl;
        }
    }

    // 查询所有点
    Rect entireTree(0, 0, 100, 100);
    auto allPoints = qt.query(entireTree);
    std::cout << "\nPoints in tree before deletion: " << allPoints.size() << std::endl;
    for (const auto &p : allPoints)
    {
        std::cout << "(" << p.x << ", " << p.y << ") ";
    }
    std::cout << std::endl;

    // 删除一个点，例如 (30, 30)
    Point pointToRemove(30, 30);
    std::cout << "\nAttempting to remove point (" << pointToRemove.x << ", " << pointToRemove.y << ")." << std::endl;
    bool removalStatus = qt.remove(pointToRemove);
    if (removalStatus)
    {
        std::cout << "Point removed successfully." << std::endl;
    }
    else
    {
        std::cout << "Point not found or could not be removed." << std::endl;
    }

    // 再次查询所有点，确认删除
    allPoints = qt.query(entireTree);
    std::cout << "\nPoints in tree after deletion: " << allPoints.size() << std::endl;
    for (const auto &p : allPoints)
    {
        std::cout << "(" << p.x << ", " << p.y << ") ";
    }
    std::cout << std::endl;

    return 0;
}