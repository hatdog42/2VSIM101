using System.Globalization;

Triangulation triangulation = new();
triangulation.Start();

struct Point
{
    public double x, y, z;
}

struct Triangle
{
    public int indiceA, indiceB, indiceC;
    public int neighbourA, neighbourB, neighbourC;
}


class Triangulation
{
    private const double epsilon = 1e-9;

    private int pointCount;
    private Point[] points;
    
    private Point[] vertices;
    private List<Triangle> triangles = new();
    
    private List<int> convexHull;

    public void Start()
    {
        ReadFile(@"C:\Users\krist\RiderProjects\Triangulation\lasdata.txt");
        PutPointInToCells(1.0);
        convexHull = CreateConvexHull(vertices);
        triangles = CreateInaitalTriagulation(convexHull);
        LegalizeInitialTriangulation();
        TriangulatePoints();
        WriteTriangles("triangulation.txt");
    }
    
    void ReadFile(string filename)
    {
        using StreamReader reader = new StreamReader(filename);
    
        pointCount = int.Parse(reader.ReadLine()!);
        points = new Point[pointCount];
        
        for (int i = 0; i < pointCount; i++)
        {
            string line = reader.ReadLine()!;
            string[] values = line.Split(
                ' ',
                StringSplitOptions.RemoveEmptyEntries
            );

            points[i].x = double.Parse(values[0], CultureInfo.InvariantCulture);
            points[i].y = double.Parse(values[1], CultureInfo.InvariantCulture);
            points[i].z = double.Parse(values[2], CultureInfo.InvariantCulture);
        }
    }

    void PutPointInToCells(double cellSize)
    {
        Dictionary<(int x, int y), Point> cells = new(); //hash map

        double originX = points[0].x;
        double originY = points[0].y;

        foreach (Point point in points)
        {
            //split into cells
            int cellX = (int)Math.Floor((point.x - originX) / cellSize);
            int cellY = (int)Math.Floor((point.y - originY) / cellSize);

            var cell = (cellX, cellY);

            if (cells.TryAdd(cell, point)) {}
            else if (point.z < cells[cell].z)
            {
                cells[cell] = point;
            }
        }

        // write new points to file
        vertices = cells.Values.ToArray();
        var lines = new List<string>
        {
            vertices.Length.ToString(CultureInfo.InvariantCulture) // CultureInfo.InvariantCulture = use . insted of ,
        };
        foreach (Point point in vertices)
        {
            lines.Add(
                $"{point.x.ToString(CultureInfo.InvariantCulture)} " +
                $"{point.y.ToString(CultureInfo.InvariantCulture)} " +
                $"{point.z.ToString(CultureInfo.InvariantCulture)}"
            );
        }

        File.WriteAllLines("vertices.txt", lines);
    }

    void TriangulatePoints()
    {
        HashSet<int> hullPoints = convexHull.ToHashSet();

        for (int i = 0; i < vertices.Length; i++)
        {
            // Already used to make the initial triangulation
            if (hullPoints.Contains(i))
                continue;

            int triangleIndex =
                FindContainingTriangle(vertices[i]);

            if (triangleIndex == -1)
                continue;

            SplitTriangle(triangleIndex, i);
        }
    }
    
    List<int> CreateConvexHull(Point[] vertices)
    {
        int[] sorted = Enumerable.Range(0, vertices.Length)
            .OrderBy(i => vertices[i].x)
            .ThenBy(i => vertices[i].y)
            .ToArray();

        List<int> lower = new();

        foreach (int index in sorted)
        {
            while (lower.Count >= 2)
            {
                int a = lower[lower.Count - 2];
                int b = lower[lower.Count - 1];

                if (CrossProduct(
                        vertices[a],
                        vertices[b],
                        vertices[index]) > epsilon)
                {
                    break;
                }

                lower.RemoveAt(lower.Count - 1);
            }

            lower.Add(index);
        }

        List<int> upper = new();

        for (int i = sorted.Length - 1; i >= 0; i--)
        {
            int index = sorted[i];

            while (upper.Count >= 2)
            {
                int a = upper[upper.Count - 2];
                int b = upper[upper.Count - 1];

                if (CrossProduct(
                        vertices[a],
                        vertices[b],
                        vertices[index]) > epsilon)
                {
                    break;
                }

                upper.RemoveAt(upper.Count - 1);
            }

            upper.Add(index);
        }

        lower.RemoveAt(lower.Count - 1);
        upper.RemoveAt(upper.Count - 1);

        lower.AddRange(upper);

        return lower;
    }

    List<Triangle> CreateInaitalTriagulation(List<int> hull)
    {
        List<Triangle> triangles = new();
        int triangleCount = hull.Count - 2;
        
        for (int i = 1; i < hull.Count - 1; i++)
        {
            int triangleIndex = i - 1;
            
            Triangle triangle = new();

            triangle.indiceA = hull[0];
            triangle.indiceB = hull[i];
            triangle.indiceC = hull[i + 1];
            
            triangle.neighbourA = -1; //outside bounds of convexHull
            
            if (triangleIndex < triangleCount - 1)
                triangle.neighbourB = triangleIndex + 1;
            else
                triangle.neighbourB = -1;

            if (triangleIndex > 0)
                triangle.neighbourC = triangleIndex - 1;
            else
                triangle.neighbourC = -1;
            
            triangles.Add(triangle);
        }
        return triangles;
    }
    
    void LegalizeInitialTriangulation()
    {
        bool changed = true;

        while (changed)
        {
            changed = false;

            for (int i = 0; i < triangles.Count; i++)
            {
                Triangle triangle = triangles[i];

                int[] neighbours =
                {
                    triangle.neighbourA,
                    triangle.neighbourB,
                    triangle.neighbourC
                };

                foreach (int neighbourIndex in neighbours)
                {
                    if (neighbourIndex == -1)
                        continue;

                    // Don't check the same pair twice
                    if (neighbourIndex < i)
                        continue;

                    if (LegalizeTrianglePair(i, neighbourIndex))
                    {
                        changed = true;
                        break;
                    }
                }

                // A flip changed the topology,
                // so restart from triangle 0
                if (changed)
                    break;
            }
        }
    }

    bool IsPointInsideTriangle(Point point, Triangle triangle)
    {
        GetTriangleSides(point, triangle, out double ab, out double bc, out double ca);
        
        return ab >= -epsilon && bc >= -epsilon && ca >= -epsilon;
    }
    
    bool IsInsideCircumcircle(Point a, Point b, Point c, Point p)
    {
        double ax = a.x - p.x;
        double ay = a.y - p.y;

        double bx = b.x - p.x;
        double by = b.y - p.y;

        double cx = c.x - p.x;
        double cy = c.y - p.y;

        double determinant =
            (ax * ax + ay * ay) * (bx * cy - cx * by)
            - (bx * bx + by * by) * (ax * cy - cx * ay)
            + (cx * cx + cy * cy) * (ax * by - bx * ay);

        double scale =
            (ax * ax + ay * ay) * (Math.Abs(bx * cy) + Math.Abs(cx * by))
            + (bx * bx + by * by) * (Math.Abs(ax * cy) + Math.Abs(cx * ay))
            + (cx * cx + cy * cy) * (Math.Abs(ax * by) + Math.Abs(bx * ay));

        return determinant > 1e-14 * scale;
    }
    
    int FindContainingTriangle(Point point)
    {
        if (triangles.Count == 0)
            return -1;

        int current = 0;

        for (int step = 0; step < triangles.Count; step++)
        {
            Triangle triangle = triangles[current];

            GetTriangleSides(point, triangle, out double ab, out double bc, out double ca);

            if (ab >= -epsilon &&
                bc >= -epsilon &&
                ca >= -epsilon)
            {
                return current;
            }

            int nextTriangle;

            if (bc <= ca && bc <= ab)
            {
                nextTriangle = triangle.neighbourA;
            }
            else if (ca <= ab && ca <= bc)
            {
                nextTriangle = triangle.neighbourB;
            }
            else
            {
                nextTriangle = triangle.neighbourC;
            }

            if (nextTriangle == -1)
                return -1;

            current = nextTriangle;
        }

        return -1;
    }
    
    void SplitTriangle(int triangleIndex, int pointIndex)
    {
        Triangle old = triangles[triangleIndex];

        Point point = vertices[pointIndex];

        if (Math.Abs(CrossProduct(vertices[old.indiceA], vertices[old.indiceB], point)) <= epsilon)
        {
            SplitEdge(triangleIndex, pointIndex, old.indiceA, old.indiceB);
            return;
        }

        if (Math.Abs(CrossProduct(vertices[old.indiceB], vertices[old.indiceC], point)) <= epsilon)
        {
            SplitEdge(triangleIndex, pointIndex, old.indiceB, old.indiceC);
            return;
        }

        if (Math.Abs(CrossProduct(vertices[old.indiceC], vertices[old.indiceA], point)) <= epsilon)
        {
            SplitEdge(triangleIndex, pointIndex, old.indiceC, old.indiceA);
            return;
        }

        int t0Index = triangleIndex;
        int t1Index = triangles.Count;
        int t2Index = triangles.Count + 1;

        Triangle t0 = new();
        Triangle t1 = new();
        Triangle t2 = new();

        t0.indiceA = old.indiceA;
        t0.indiceB = old.indiceB;
        t0.indiceC = pointIndex;

        t1.indiceA = old.indiceB;
        t1.indiceB = old.indiceC;
        t1.indiceC = pointIndex;

        t2.indiceA = old.indiceC;
        t2.indiceB = old.indiceA;
        t2.indiceC = pointIndex;

        t0.neighbourA = t1Index;
        t0.neighbourB = t2Index;
        t0.neighbourC = old.neighbourC;

        t1.neighbourA = t2Index;
        t1.neighbourB = t0Index;
        t1.neighbourC = old.neighbourA;

        t2.neighbourA = t0Index;
        t2.neighbourB = t1Index;
        t2.neighbourC = old.neighbourB;

        triangles[triangleIndex] = t0;
        triangles.Add(t1);
        triangles.Add(t2);
        
        ReplaceNeighbour(old.neighbourA, triangleIndex, t1Index);
        ReplaceNeighbour(old.neighbourB, triangleIndex, t2Index);
        
        LegalizeEdge(t0Index, pointIndex);
        LegalizeEdge(t1Index, pointIndex);
        LegalizeEdge(t2Index, pointIndex);
    }

    void SplitEdge(int triangleIndex, int pointIndex, int a, int b)
    {
        Triangle old = triangles[triangleIndex];
        int c = GetOppositeVertex(old, a, b);
        int neighbourIndex = GetNeighbourAcrossEdge(old, a, b);

        int t0Index = triangleIndex;
        int t1Index = triangles.Count;
        int t2Index = neighbourIndex;
        int t3Index = neighbourIndex == -1 ? -1 : triangles.Count + 1;

        Triangle t0 = new();
        Triangle t1 = new();

        t0.indiceA = a;
        t0.indiceB = pointIndex;
        t0.indiceC = c;

        t1.indiceA = pointIndex;
        t1.indiceB = b;
        t1.indiceC = c;

        t0.neighbourA = t1Index;
        t0.neighbourB = GetNeighbourAcrossEdge(old, c, a);
        t0.neighbourC = t3Index;

        t1.neighbourA = GetNeighbourAcrossEdge(old, b, c);
        t1.neighbourB = t0Index;
        t1.neighbourC = t2Index;

        triangles[t0Index] = t0;
        triangles.Add(t1);

        ReplaceNeighbour(t1.neighbourA, triangleIndex, t1Index);

        // A shared edge needs both adjacent triangles split before legalization.
        if (neighbourIndex != -1)
        {
            Triangle neighbour = triangles[neighbourIndex];
            int d = GetOppositeVertex(neighbour, a, b);

            Triangle t2 = new();
            Triangle t3 = new();

            t2.indiceA = b;
            t2.indiceB = pointIndex;
            t2.indiceC = d;

            t3.indiceA = pointIndex;
            t3.indiceB = a;
            t3.indiceC = d;

            t2.neighbourA = t3Index;
            t2.neighbourB = GetNeighbourAcrossEdge(neighbour, d, b);
            t2.neighbourC = t1Index;

            t3.neighbourA = GetNeighbourAcrossEdge(neighbour, a, d);
            t3.neighbourB = t2Index;
            t3.neighbourC = t0Index;

            triangles[t2Index] = t2;
            triangles.Add(t3);

            ReplaceNeighbour(t3.neighbourA, neighbourIndex, t3Index);
        }

        LegalizeEdge(t0Index, pointIndex);
        LegalizeEdge(t1Index, pointIndex);

        if (neighbourIndex != -1)
        {
            LegalizeEdge(t2Index, pointIndex);
            LegalizeEdge(t3Index, pointIndex);
        }
    }

    void FlipEdge(int triangleIndex, int neighbourIndex)
    {
        Triangle triangle = triangles[triangleIndex];
        Triangle neighbour = triangles[neighbourIndex];

        int sharedA = -1;
        int sharedB = -1;

        int[] triangleVertices =
        {
            triangle.indiceA,
            triangle.indiceB,
            triangle.indiceC
        };

        foreach (int vertex in triangleVertices)
        {
            if (HasVertex(neighbour, vertex))
            {
                if (sharedA == -1)
                    sharedA = vertex;
                else
                    sharedB = vertex;
            }
        }

        int p = GetOppositeVertex(
            triangle,
            sharedA,
            sharedB);

        int d = GetOppositeVertex(
            neighbour,
            sharedA,
            sharedB);

        if (CrossProduct(
                vertices[sharedA],
                vertices[sharedB],
                vertices[p]) < -epsilon)
        {
            (sharedA, sharedB) = (sharedB, sharedA);
        }

        int a = sharedA;
        int b = sharedB;

        int neighbourBP =
            GetNeighbourAcrossEdge(triangle, b, p);

        int neighbourPA =
            GetNeighbourAcrossEdge(triangle, p, a);

        int neighbourAD =
            GetNeighbourAcrossEdge(neighbour, a, d);

        int neighbourDB =
            GetNeighbourAcrossEdge(neighbour, d, b);


        Triangle newTriangle = new();

        newTriangle.indiceA = p;
        newTriangle.indiceB = a;
        newTriangle.indiceC = d;

        newTriangle.neighbourA = neighbourAD;
        newTriangle.neighbourB = neighbourIndex;
        newTriangle.neighbourC = neighbourPA;


        Triangle newNeighbour = new();

        newNeighbour.indiceA = p;
        newNeighbour.indiceB = d;
        newNeighbour.indiceC = b;

        newNeighbour.neighbourA = neighbourDB;
        newNeighbour.neighbourB = neighbourBP;
        newNeighbour.neighbourC = triangleIndex;


        triangles[triangleIndex] = newTriangle;
        triangles[neighbourIndex] = newNeighbour;


        ReplaceNeighbour(
            neighbourAD,
            neighbourIndex,
            triangleIndex);

        ReplaceNeighbour(
            neighbourBP,
            triangleIndex,
            neighbourIndex);
    }
    void LegalizeEdge(int triangleIndex, int pointIndex)
    {
        Triangle triangle = triangles[triangleIndex];

        int neighbourIndex;

        if (triangle.indiceA == pointIndex)
        {
            neighbourIndex = triangle.neighbourA;
        }
        else if (triangle.indiceB == pointIndex)
        {
            neighbourIndex = triangle.neighbourB;
        }
        else if (triangle.indiceC == pointIndex)
        {
            neighbourIndex = triangle.neighbourC;
        }
        else
        {
            return;
        }

        if (neighbourIndex == -1)
            return;

        if (LegalizeTrianglePair(triangleIndex, neighbourIndex))
        {
            LegalizeEdge(triangleIndex, pointIndex);
            LegalizeEdge(neighbourIndex, pointIndex);
        }
    }
    bool LegalizeTrianglePair(int triangleIndex, int neighbourIndex)
    {
        if (neighbourIndex == -1)
            return false;

        Triangle triangle = triangles[triangleIndex];
        Triangle neighbour = triangles[neighbourIndex];

        int sharedA = -1;
        int sharedB = -1;

        int[] triangleVertices =
        {
            triangle.indiceA,
            triangle.indiceB,
            triangle.indiceC
        };

        foreach (int vertex in triangleVertices)
        {
            if (HasVertex(neighbour, vertex))
            {
                if (sharedA == -1)
                    sharedA = vertex;
                else
                    sharedB = vertex;
            }
        }

        // They are not actually neighbours
        if (sharedA == -1 || sharedB == -1)
            return false;

        int oppositeVertex =
            GetOppositeVertex(
                neighbour,
                sharedA,
                sharedB);

        if (oppositeVertex == -1)
            return false;

        bool illegal = IsInsideCircumcircle(
            vertices[triangle.indiceA],
            vertices[triangle.indiceB],
            vertices[triangle.indiceC],
            vertices[oppositeVertex]);

        if (!illegal)
            return false;

        FlipEdge(triangleIndex, neighbourIndex);

        return true;
    }
    bool SameEdge(int a, int b, int c, int d)
    {
        return (a == c && b == d) ||
               (a == d && b == c);
    }
    int GetNeighbourAcrossEdge(Triangle triangle, int edgeA, int edgeB)
    {
        // neighbourA is across B-C
        if (SameEdge(
                edgeA, edgeB,
                triangle.indiceB, triangle.indiceC))
        {
            return triangle.neighbourA;
        }

        // neighbourB is across C-A
        if (SameEdge(
                edgeA, edgeB,
                triangle.indiceC, triangle.indiceA))
        {
            return triangle.neighbourB;
        }

        // neighbourC is across A-B
        if (SameEdge(
                edgeA, edgeB,
                triangle.indiceA, triangle.indiceB))
        {
            return triangle.neighbourC;
        }

        return -1;
    }
    bool HasVertex(Triangle triangle, int vertex)
    {
        return triangle.indiceA == vertex ||
               triangle.indiceB == vertex ||
               triangle.indiceC == vertex;
    }
    int GetOppositeVertex(Triangle neighbour, int edgeA, int edgeB)
    {
        if (neighbour.indiceA != edgeA && neighbour.indiceA != edgeB)
            return neighbour.indiceA;

        if (neighbour.indiceB != edgeA && neighbour.indiceB != edgeB)
            return neighbour.indiceB;

        if (neighbour.indiceC != edgeA && neighbour.indiceC != edgeB)
            return neighbour.indiceC;

        return -1;
    }
    void ReplaceNeighbour(int triangleIndex, int oldNeighbour, int newNeighbour)
    {
        if (triangleIndex == -1) return;

        Triangle triangle = triangles[triangleIndex];

        if (triangle.neighbourA == oldNeighbour)
        {
            triangle.neighbourA = newNeighbour;
        }
        else if (triangle.neighbourB == oldNeighbour)
        {
            triangle.neighbourB = newNeighbour;
        }
        else if (triangle.neighbourC == oldNeighbour)
        {
            triangle.neighbourC = newNeighbour;
        }

        triangles[triangleIndex] = triangle;
    }
    void GetTriangleSides(Point point, Triangle triangle, out double ab, out double bc, out double ca)
    {
        Point a = vertices[triangle.indiceA];
        Point b = vertices[triangle.indiceB];
        Point c = vertices[triangle.indiceC];

        ab = CrossProduct(a, b, point);
        bc = CrossProduct(b, c, point);
        ca = CrossProduct(c, a, point);
    }
    double CrossProduct(Point a, Point b, Point c)
    {
        return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    }
    void WriteTriangles(string filename)
    {
        var lines = new List<string>
        {
            triangles.Count.ToString(CultureInfo.InvariantCulture)
        };

        foreach (Triangle triangle in triangles)
        {
            lines.Add(
                $"{triangle.indiceA} {triangle.indiceB} {triangle.indiceC} " +
                $"{triangle.neighbourA} {triangle.neighbourB} {triangle.neighbourC}"
            );
        }

        File.WriteAllLines(filename, lines);
    }
}
