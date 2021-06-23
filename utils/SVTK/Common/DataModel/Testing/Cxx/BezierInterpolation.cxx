#include "svtkBezierInterpolation.h"

#include "svtkMultiBaselineRegressionTest.h"

#include "svtkDoubleArray.h"
#include "svtkNew.h"
#include "svtkSmartPointer.h"
#include "svtkTable.h"

#include "svtkCellArray.h"
#include "svtkPointData.h"
#include "svtkPoints.h"
#include "svtkPolyData.h"
#include "svtkVector.h"
#include "svtkVectorOperators.h"
#include "svtkXMLPolyDataWriter.h"

#include "svtkAxis.h"
#include "svtkChartXY.h"
#include "svtkColor.h"
#include "svtkColorSeries.h"
#include "svtkContextScene.h"
#include "svtkContextView.h"
#include "svtkFloatArray.h"
#include "svtkPlot.h"
#include "svtkRegressionTestImage.h"
#include "svtkRenderWindow.h"
#include "svtkRenderWindowInteractor.h"
#include "svtkTable.h"

#include <sstream>
#include <vector>

#include <math.h>

#include "svtkTestConditionals.txx"

using namespace svtk;

template <int N, typename T, typename U>
void printShape(const int order[N], const T& rst, U& shape)
{
  if (N != 3 || order[0] != 2 || order[1] != 2 || order[2] != 2)
  {
    return;
  }

  std::cout << "  ";
  for (int i = 0; i < 3; ++i)
  {
    std::cout.width(4);
    std::cout << " " << order[i];
  }
  std::cout << "  / (";
  for (int i = 0; i < 3; ++i)
  {
    std::cout.width(3);
    std::cout << " " << (rst[i] >= 0 ? "+" : "") << rst[i];
  }
  std::cout << " ) :";
  typename U::const_iterator it;
  for (it = shape.begin(); it != shape.end(); ++it)
  {
    std::cout << " " << (*it > 0 ? "+" : "") << *it;
  }
  std::cout << "\n";
}

template <typename T>
bool testShape(const int order[3], T& shape, int nonzeroidx)
{
  typename T::const_iterator it;
  int i = 0;
  bool ok = true;

  if (order[0] != 2 || order[1] != 2 || order[2] != 2)
  {
    return ok;
  }

  for (it = shape.begin(); it != shape.end(); ++it, ++i)
  {
    bool tval = fabs(*it - (i == nonzeroidx ? 1.0 : 0.0)) < 1e-7;
    if (!tval)
    {
      std::cerr << "  ERROR: Expected shape[" << i << "] == " << (i == nonzeroidx ? 1 : 0)
                << " got " << *it << "\n";
    }
    ok &= tval;
  }
  return ok;
}

template <typename T, typename U>
void InsertTableRow(int& row, T& all, const U& shape)
{
  typename T::iterator ait = all.begin();
  (*ait)->SetValue(row, row);
  ++ait; // Advance past "X" column

  int delta = 0;
  typename U::const_iterator sit;
  for (sit = shape.begin(); ait != all.end() && sit != shape.end(); ++ait, ++sit, ++delta)
  {
    (*ait)->SetValue(row, *sit + 2.0 * delta);
  }
  ++row;
}

template <int N, typename T, typename U>
svtkSmartPointer<svtkTable> CreateShapeFunctionTable(const int order[N], T& testpts, U method)
{
  svtkIdType numtests = testpts->GetNumberOfPoints();

  svtkNew<svtkBezierInterpolation> interp;
  svtkSmartPointer<svtkTable> out = svtkSmartPointer<svtkTable>::New();
  int ss = 1;
  for (int oo = 0; oo < N; ++oo)
  {
    ss *= (order[oo] + 1);
  }
  int nd = 16;
  int nn = nd * (numtests - 1) + 1;
  double dd = 1. / nd;
  svtkNew<svtkDoubleArray> xcol;
  svtkNew<svtkDoubleArray> rst;
  xcol->SetName("X");
  xcol->SetNumberOfTuples(nn);
  rst->SetName("rst");
  rst->SetNumberOfComponents(3);
  rst->SetNumberOfTuples(nn);
  out->AddColumn(xcol.GetPointer());
  out->GetFieldData()->AddArray(rst.GetPointer());

  std::vector<double> shape(ss);
  std::vector<svtkDoubleArray*> all(ss + 1);
  all[0] = xcol.GetPointer();
  for (int i = 0; i < ss; ++i)
  {
    svtkNew<svtkDoubleArray> col;
    std::ostringstream cname;
    cname << "Shape " << i;
    col->SetName(cname.str().c_str());
    col->SetNumberOfTuples(nn);
    all[i + 1] = col.GetPointer();
    out->AddColumn(col.GetPointer());
  }

  bool ok = true;
  int row = 0;
  for (unsigned i = 0; i < static_cast<unsigned>(numtests); ++i)
  {
    svtkVector3d pcoord(testpts->GetPoint(i));
    method(order, pcoord.GetData(), &shape[0]);
    printShape<N>(order, pcoord, shape);
    ok &= testShape(order, shape, i);
    rst->SetTuple(row, pcoord.GetData());
    InsertTableRow(row, all, shape);

    if (i + 1 < static_cast<unsigned>(numtests))
    { // Interpolate between this test point and next, saving to output table.
      svtkVector3d p0(testpts->GetPoint(i + 1));
      svtkVector3d p1(testpts->GetPoint(i));
      for (double xx = dd; xx < 1.0; xx += dd)
      {
        svtkVector3d xp = p0 * xx + p1 * (1. - xx);
        method(order, xp.GetData(), &shape[0]);
        rst->SetTuple(row, xp.GetData());
        InsertTableRow(row, all, shape);
      }
    }
  }
  if (!ok)
  {
    std::cerr << "ERROR: Failed test\n";
  }
  return out;
}

template <int N, typename T>
svtkSmartPointer<svtkPoints> CreatePrismaticPoints(const int order[N], T method)
{
  svtkSmartPointer<svtkPoints> pts;
  method(pts, order);

  svtkVector3d pt;
  for (int ii = 0; ii < pts->GetNumberOfPoints(); ++ii)
  {
    pts->GetPoint(ii, pt.GetData());
    // std::cout << "  " << ii << " " << pt[0] << " " << pt[1] << " " << pt[2] << "\n";
  }
  return pts;
}

svtkSmartPointer<svtkPoints> CreateHexPoints(int order[3])
{
  svtkSmartPointer<svtkPoints> pts;
  svtkBezierInterpolation::AppendHexahedronCollocationPoints(pts, order);

  svtkVector3d pt;
  for (int ii = 0; ii < pts->GetNumberOfPoints(); ++ii)
  {
    pts->GetPoint(ii, pt.GetData());
    // std::cout << "  " << ii << " " << pt[0] << " " << pt[1] << " " << pt[2] << "\n";
  }
  return pts;
}

template <typename T>
bool TestShapeFunctionImage(T& tab, int argc, char* argv[], const std::string& testImg)
{
  bool imageTest = !testImg.empty();
  bool interact = false;
  for (int a = 0; a < argc; ++a)
  {
    if (!strcmp(argv[a], "-I"))
    {
      interact = true;
    }
  }
  // Set up a 2D scene, add an XY chart to it
  svtkNew<svtkContextView> view;
  view->GetRenderWindow()->SetSize(800, 600);
  svtkNew<svtkChartXY> chart;
  view->GetScene()->AddItem(chart.GetPointer());

  svtkNew<svtkColorSeries> colors;
  colors->SetColorScheme(svtkColorSeries::BREWER_QUALITATIVE_DARK2);
  int nc = colors->GetNumberOfColors();

  std::vector<svtkPlot*> plots;
  for (int c = 1; c < tab->GetNumberOfColumns(); ++c)
  {
    svtkPlot* plot = chart->AddPlot(svtkChart::LINE);
    svtkColor3ub cu = colors->GetColor((c - 1) % nc);
    plot->SetInputData(tab.GetPointer(), 0, c);
    plot->SetColor(cu.GetRed(), cu.GetGreen(), cu.GetBlue(), 255);
    plot->SetWidth(2.0);
    plots.push_back(plot);
  }

  chart->GetAxis(svtkAxis::LEFT)->SetTickLabelAlgorithm(svtkAxis::TICK_WILKINSON_EXTENDED);
  chart->GetAxis(svtkAxis::BOTTOM)->SetTickLabelAlgorithm(svtkAxis::TICK_WILKINSON_EXTENDED);
  view->GetRenderWindow()->SetMultiSamples(0);
  view->GetInteractor()->Initialize();
  view->GetRenderWindow()->Render();
  if (imageTest)
  {
    int retVal = RegressionTestWithImageName(argc, argv, view->GetRenderWindow(), testImg);
    if (retVal == svtkRegressionTester::DO_INTERACTOR)
    {
      view->GetRenderWindow()->SetMultiSamples(8);
      view->GetInteractor()->Start();
    }
    return retVal ? true : false;
  }
  if (interact)
  {
    view->GetRenderWindow()->SetMultiSamples(8);
    view->GetInteractor()->Start();
  }
  return true; // If asked not to do a regression test, we always succeed.
}

template <int N, typename T, typename U, typename V>
bool SumShapeFunctions(const int order[N], T tab, U world, V lpd)
{
  svtkDataArray* rst = tab->GetFieldData()->GetArray("rst");
  svtkIdType nr = tab->GetNumberOfRows();
  svtkIdType nc = tab->GetNumberOfColumns();
  svtkNew<svtkBezierInterpolation> interp;
  svtkNew<svtkPoints> ppt;
  svtkNew<svtkCellArray> polyline;
  svtkNew<svtkDoubleArray> time;
  svtkNew<svtkDoubleArray> du;
  svtkNew<svtkDoubleArray> dv;
  svtkNew<svtkDoubleArray> dw;
  du->SetName("dr");
  dv->SetName("ds");
  dw->SetName("dt");
  du->SetNumberOfComponents(3);
  dv->SetNumberOfComponents(3);
  dw->SetNumberOfComponents(3);
  du->SetNumberOfTuples(nr);
  dv->SetNumberOfTuples(nr);
  dw->SetNumberOfTuples(nr);
  ppt->SetNumberOfPoints(nr);
  time->SetName("T");
  time->SetNumberOfTuples(nr);
  std::vector<svtkIdType> conn;
  std::vector<double> derivs(N * (nc - 1)); // nc - 1 => skip the column holding the row number
  for (svtkIdType rr = 0; rr < nr; ++rr)
  {
    svtkVector3d pt(0, 0, 0); // Interpolated result point
    svtkVector3d dr[N];       // Interpolated derivatives
    for (int i = 0; i < N; ++i)
    {
      for (int j = 0; j < 3; ++j)
      {
        dr[i][j] = 0.0;
      }
    }
    svtkVector3d cp; // Bezier control point
    cp[2] = 0.0;
    svtkVector3d uu; // Parametric coordinates of control point

    rst->GetTuple(rr, uu.GetData());
    switch (N)
    {
      case 1:
        interp->Tensor1ShapeDerivatives(order, uu.GetData(), &derivs[0]);
        break;
      case 2:
        interp->Tensor2ShapeDerivatives(order, uu.GetData(), &derivs[0]);
        break;
      case 3:
        interp->Tensor3ShapeDerivatives(order, uu.GetData(), &derivs[0]);
        break;
      default:
        std::cerr << "Unsupported dimension " << N << ". No way to obtain derivatives.\n";
        break;
    }
    // std::cout << "r = " << uu[0] << " " << uu[1] << " " << uu[2] << "\n";
    for (int cc = 1; cc < nc; ++cc)
    {
      world->GetPoint(cc - 1, cp.GetData());
      /*
      if (rr < 2) {
        std::cout
          << "cc " << cc << " (" << cp[0] << " " << cp[1] << " " << cp[2] << ")  "
          << (tab->GetValue(rr, cc).ToDouble() - 2. * cc + 2) << "\n";
      }
      */
      // The "- 2.0 * cc + 2.0" accounts for the amount added to each
      // column to make the 2-D plot of shape functions simpler:
      pt = pt + (tab->GetValue(rr, cc).ToDouble() - 2.0 * cc + 2.0) * cp;

      dr[0] = dr[0] + derivs[N * (cc - 1)] * cp;
      // std::cout << "  dr += " << derivs[N * (cc - 1)] << " * (" << cp[0] << " " << cp[1] << " "
      // << cp[2] << ")\n";
      if (N > 1)
      {
        dr[1] = dr[1] + derivs[N * (cc - 1) + 1] * cp;
        if (N > 2)
        {
          dr[2] = dr[2] + derivs[N * (cc - 1) + 2] * cp;
        }
      }
    }
    conn.push_back(rr);
    ppt->SetPoint(rr, pt.GetData());
    du->SetTuple(rr, dr[0].GetData());
    // std::cout << "  dr = (" << cp[0] << " " << cp[1] << " " << cp[2] << ")\n";
    if (N > 1)
    {
      dv->SetTuple(rr, dr[1].GetData());
      if (N > 2)
      {
        dw->SetTuple(rr, dr[2].GetData());
      }
    }
    time->SetValue(rr, rr);
  }
  polyline->InsertNextCell(static_cast<svtkIdType>(conn.size()), &conn[0]);
  lpd->Initialize();
  lpd->SetPoints(ppt.GetPointer());
  lpd->SetLines(polyline.GetPointer());
  lpd->GetPointData()->SetScalars(time.GetPointer());
  lpd->GetPointData()->AddArray(du.GetPointer());
  if (N > 1)
  {
    lpd->GetPointData()->AddArray(dv.GetPointer());
    if (N > 2)
    {
      lpd->GetPointData()->AddArray(dw.GetPointer());
    }
  }
  return true;
}

template <int N, typename T, typename U, typename V>
bool SumWedgeShapeFunctions(const int order[N], T tab, U world, V lpd)
{
  (void)order;
  svtkDataArray* rst = tab->GetFieldData()->GetArray("rst");
  svtkIdType nr = tab->GetNumberOfRows();
  svtkIdType nc = tab->GetNumberOfColumns();
  svtkNew<svtkBezierInterpolation> interp;
  svtkNew<svtkPoints> ppt;
  svtkNew<svtkCellArray> polyline;
  svtkNew<svtkDoubleArray> time;
  ppt->SetNumberOfPoints(nr);
  time->SetName("T");
  time->SetNumberOfTuples(nr);
  std::vector<svtkIdType> conn;
  for (svtkIdType rr = 0; rr < nr; ++rr)
  {
    svtkVector3d pt(0, 0, 0); // Interpolated result point
    svtkVector3d cp;          // Bezier control point
    cp[2] = 0.0;
    svtkVector3d uu; // Parametric coordinates of control point

    rst->GetTuple(rr, uu.GetData());
    // std::cout << "r = " << uu[0] << " " << uu[1] << " " << uu[2] << "\n";
    for (int cc = 1; cc < nc; ++cc)
    {
      world->GetPoint(cc - 1, cp.GetData());
      /*
      if (rr < 2) {
        std::cout
          << "cc " << cc << " (" << cp[0] << " " << cp[1] << " " << cp[2] << ")  "
          << (tab->GetValue(rr, cc).ToDouble() - 2. * cc + 2) << "\n";
      }
      */
      // The "- 2.0 * cc + 2.0" accounts for the amount added to each
      // column to make the 2-D plot of shape functions simpler:
      pt = pt + (tab->GetValue(rr, cc).ToDouble() - 2.0 * cc + 2.0) * cp;
    }
    conn.push_back(rr);
    ppt->SetPoint(rr, pt.GetData());
    time->SetValue(rr, rr);
  }
  polyline->InsertNextCell(static_cast<svtkIdType>(conn.size()), &conn[0]);
  lpd->Initialize();
  lpd->SetPoints(ppt.GetPointer());
  lpd->SetLines(polyline.GetPointer());
  lpd->GetPointData()->SetScalars(time.GetPointer());
  return true;
}

#if 0
static bool SnapFace(svtkPoints* pts)
{
  int face1PtIds[] = {
    1, 2, 5, 6,
    9, 13, 17, 18,
    21
  };
  /*
  double face1PtDeltaX[] = {
    -0.20, -0.20, -0.20, -0.20,
    -0.10, -0.10, -0.10, -0.10,
    0.0
  };
  double face1PtDeltaY[] = {
    -0.20, +0.20, -0.20, +0.20,
     0.00,  0.00, -0.10, +0.10,
    0.0
  };
  double face1PtDeltaZ[] = {
    -0.20, -0.20, +0.20, +0.20,
    -0.10, +0.10,  0.00,  0.00,
    0.0
  };
  */
  double face1PtDeltaX[] = {
    -0.10, -0.10, -0.10, -0.10,
    -0.05, -0.05, -0.05, -0.05,
    0.0
  };
  double face1PtDeltaY[] = {
    -0.10, +0.10, -0.10, +0.10,
     0.00,  0.00, -0.05, +0.05,
    0.0
  };
  double face1PtDeltaZ[] = {
    -0.10, -0.10, +0.10, +0.10,
    -0.05, +0.05,  0.00,  0.00,
    0.0
  };
  svtkVector3d xx;
  for (unsigned ii = 0; ii < sizeof(face1PtIds) / sizeof(face1PtIds[0]); ++ii)
    {
    pts->GetPoint(face1PtIds[ii], xx.GetData());
    xx[0] += face1PtDeltaX[ii];
    xx[1] += face1PtDeltaY[ii];
    xx[2] += face1PtDeltaZ[ii];
    pts->SetPoint(face1PtIds[ii], xx.GetData());
    }
  return true;
}
#endif

template <typename T>
void Dump(T& pts)
{
  svtkIdType np = pts->GetNumberOfPoints();
  for (svtkIdType i = 0; i < np; ++i)
  {
    svtkVector3d p;
    pts->GetPoint(i, p.GetData());
    std::cout << "  " << i << "  " << p[0] << " " << p[1] << " " << p[2] << "\n";
  }
}

template <typename T>
bool SetupPoints(T& pts)
{
  pts->SetNumberOfPoints(2);
  pts->SetPoint(0, 0, 0, 0);
  pts->SetPoint(1, 1, 1, 0);
  pts->SetPoint(2, 2, 0, 0);
  return true;
}

int BezierInterpolation(int svtkNotUsed(argc), char* svtkNotUsed(argv)[])
{
  bool ok = true;

  svtkSmartPointer<svtkTable> tab;
  svtkSmartPointer<svtkPoints> pts;
  svtkSmartPointer<svtkPoints> world = svtkSmartPointer<svtkPoints>::New();
  svtkSmartPointer<svtkPolyData> lpd = svtkSmartPointer<svtkPolyData>::New();

  double coeffs[6];

  // linear
  svtkBezierInterpolation::EvaluateShapeFunctions(1, 0.0, coeffs);

  ok &= testNearlyEqual(coeffs[0], 1.0, "Linear lf 0");
  ok &= testNearlyEqual(coeffs[1], 0.0, "Linear lf 1");

  svtkBezierInterpolation::EvaluateShapeFunctions(1, 0.5, coeffs);

  ok &= testNearlyEqual(coeffs[0], 0.5, "Linear lf 0");
  ok &= testNearlyEqual(coeffs[1], 0.5, "Linear lf 1");

  svtkBezierInterpolation::EvaluateShapeFunctions(1, 1.0, coeffs);

  ok &= testNearlyEqual(coeffs[0], 0.0, "Linear lf 0");
  ok &= testNearlyEqual(coeffs[1], 1.0, "Linear lf 1");

  // quadratic
  svtkBezierInterpolation::EvaluateShapeFunctions(2, 0.0, coeffs);

  ok &= testNearlyEqual(coeffs[0], 1.0, "Quad lf 0");
  ok &= testNearlyEqual(coeffs[1], 0.0, "Quad lf 1");
  ok &= testNearlyEqual(coeffs[2], 0.0, "Quad lf 2");

  svtkBezierInterpolation::EvaluateShapeFunctions(2, 0.5, coeffs);

  ok &= testNearlyEqual(coeffs[0], 0.25, "Quad lf 0");
  ok &= testNearlyEqual(coeffs[1], 0.50, "Quad lf 1");
  ok &= testNearlyEqual(coeffs[2], 0.25, "Quad lf 2");

  svtkBezierInterpolation::EvaluateShapeFunctions(2, 1.0, coeffs);

  ok &= testNearlyEqual(coeffs[0], 0.0, "Quad lf 0");
  ok &= testNearlyEqual(coeffs[1], 0.0, "Quad lf 1");
  ok &= testNearlyEqual(coeffs[2], 1.0, "Quad lf 2");

  {
    const double pcoords[3] = { 1, 0, 0 };
    svtkBezierInterpolation::deCasteljauSimplex(2, 1, pcoords, coeffs);
    ok &= testNearlyEqual(coeffs[0], 0.0, "Simplex lf 0");
    ok &= testNearlyEqual(coeffs[1], 1.0, "Simplex lf 1");
    ok &= testNearlyEqual(coeffs[2], 0.0, "Simplex lf 2");
  }

  {
    const double pcoords[3] = { 0, 1, 0 };
    svtkBezierInterpolation::deCasteljauSimplex(2, 1, pcoords, coeffs);
    ok &= testNearlyEqual(coeffs[0], 0.0, "Simplex lf 0");
    ok &= testNearlyEqual(coeffs[1], 0.0, "Simplex lf 1");
    ok &= testNearlyEqual(coeffs[2], 1.0, "Simplex lf 2");
  }

  {
    const double pcoords[3] = { 0, 0, 1 };
    svtkBezierInterpolation::deCasteljauSimplex(2, 1, pcoords, coeffs);
    ok &= testNearlyEqual(coeffs[0], 1.0, "Simplex lf 0");
    ok &= testNearlyEqual(coeffs[1], 0.0, "Simplex lf 1");
    ok &= testNearlyEqual(coeffs[2], 0.0, "Simplex lf 2");
  }

  {
    const double pcoords[3] = { 1, 0, 0 };
    svtkBezierInterpolation::deCasteljauSimplex(2, 2, pcoords, coeffs);
    ok &= testNearlyEqual(coeffs[0], 0.0, "Simplex lf 0");
    ok &= testNearlyEqual(coeffs[1], 0.0, "Simplex lf 1");
    ok &= testNearlyEqual(coeffs[2], 1.0, "Simplex lf 2");
    ok &= testNearlyEqual(coeffs[3], 0.0, "Simplex lf 3");
    ok &= testNearlyEqual(coeffs[4], 0.0, "Simplex lf 4");
    ok &= testNearlyEqual(coeffs[5], 0.0, "Simplex lf 5");
    cout << "coeffs " << coeffs[0] << ", " << coeffs[1] << ", " << coeffs[2] << ", " << coeffs[3]
         << ", " << coeffs[4] << ", " << coeffs[5] << endl;
  }

  {
    const double pcoords[3] = { 0, 1, 0 };
    svtkBezierInterpolation::deCasteljauSimplex(2, 2, pcoords, coeffs);
    ok &= testNearlyEqual(coeffs[0], 0.0, "Simplex lf 0");
    ok &= testNearlyEqual(coeffs[1], 0.0, "Simplex lf 1");
    ok &= testNearlyEqual(coeffs[2], 0.0, "Simplex lf 2");
    ok &= testNearlyEqual(coeffs[3], 0.0, "Simplex lf 3");
    ok &= testNearlyEqual(coeffs[4], 0.0, "Simplex lf 4");
    ok &= testNearlyEqual(coeffs[5], 1.0, "Simplex lf 5");
    cout << "coeffs " << coeffs[0] << ", " << coeffs[1] << ", " << coeffs[2] << ", " << coeffs[3]
         << ", " << coeffs[4] << ", " << coeffs[5] << endl;
  }

  {
    const double pcoords[3] = { 0, 0, 1 };
    svtkBezierInterpolation::deCasteljauSimplex(2, 2, pcoords, coeffs);
    ok &= testNearlyEqual(coeffs[0], 1.0, "Simplex lf 0");
    ok &= testNearlyEqual(coeffs[1], 0.0, "Simplex lf 1");
    ok &= testNearlyEqual(coeffs[2], 0.0, "Simplex lf 2");
    ok &= testNearlyEqual(coeffs[3], 0.0, "Simplex lf 3");
    ok &= testNearlyEqual(coeffs[4], 0.0, "Simplex lf 4");
    ok &= testNearlyEqual(coeffs[5], 0.0, "Simplex lf 5");
    cout << "coeffs " << coeffs[0] << ", " << coeffs[1] << ", " << coeffs[2] << ", " << coeffs[3]
         << ", " << coeffs[4] << ", " << coeffs[5] << endl;
  }

  {
    const double pcoords[3] = { 0.5, 0.5, 0 };
    svtkBezierInterpolation::deCasteljauSimplex(2, 2, pcoords, coeffs);
    ok &= testNearlyEqual(coeffs[0], 0.0, "Simplex lf 0");
    ok &= testNearlyEqual(coeffs[1], 0.0, "Simplex lf 1");
    ok &= testNearlyEqual(coeffs[2], 0.25, "Simplex lf 2");
    ok &= testNearlyEqual(coeffs[3], 0.0, "Simplex lf 3");
    ok &= testNearlyEqual(coeffs[4], 0.5, "Simplex lf 4");
    ok &= testNearlyEqual(coeffs[5], 0.25, "Simplex lf 5");
    cout << "coeffs " << coeffs[0] << ", " << coeffs[1] << ", " << coeffs[2] << ", " << coeffs[3]
         << ", " << coeffs[4] << ", " << coeffs[5] << endl;
  }

  return ok ? 0 : 1;
}
