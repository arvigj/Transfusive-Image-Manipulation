//
// Created by parallels on 4/28/18.
//

#include "content_aware_bbw.h"

/*
 * Parameters: Mesh defined by V, F
 */
std::pair<Eigen::SparseMatrix<double>, Eigen::SparseMatrix<double>> LM(Eigen::MatrixXd V, Eigen::MatrixXi F) {
    Eigen::SparseMatrix<double> L(F.rows(),F.rows()), M(F.rows(),F.rows());
    std::vector<Eigen::Triplet<double>> L_tripets, M_triplets;
    for (int i=0; i<F.rows(); i++) {
        Eigen::Vector3d l, cot;
        double r, A;
        l << (V.row(F(i,0)) - V.row(F(i,1))).norm(), (V.row(F(i,1)) - V.row(F(i,2))).norm(), (V.row(F(i,2)) - V.row(F(i,0))).norm();
        r = 0.5*l.sum();
        A = std::sqrt(r*(r-l(0))*(r-l(1))*(r-l(2)));
        cot = (Eigen::Matrix3d::Ones()-2*Eigen::Matrix3d::Identity()) * l.array().square().matrix();
        cot /= A;

        std::vector<int> ijk(F.row(i).data(), F.row(i).data() + F.row(i).size());
        std::vector<int> precomputed_ab = {0,2,0,1,2,1};
        std::vector<int> precomputed_bc = {1,1,2,2,0,0};
        auto index_ab = precomputed_ab.begin();
        auto index_bc = precomputed_bc.begin();
        do{
            //L(ijk[0],ijk[1]) -= 0.5 * cot(*index_ab);
            //L(ijk[0],ijk[0]) += 0.5 * cot(*index_ab);
            L_tripets.push_back(Eigen::Triplet<double>(ijk[0], ijk[1], -0.5 * cot(*index_ab)));
            L_tripets.push_back(Eigen::Triplet<double>(ijk[0], ijk[0], 0.5 * cot(*index_ab)));
            if((cot.array() >= 0).all()) {
                //M(ijk[0],ijk[0]) += (1./8.)*cot(*index_ab)*cot(*index_ab)*l(*index_ab);
                M_triplets.push_back(Eigen::Triplet<double>(ijk[0],ijk[0],(1./8.)*cot(*index_ab)*cot(*index_ab)*l(*index_ab)));
            } else {
                //M(ijk[0],ijk[0]) += (1./8.)*A ? cot(*index_bc) >= 0 : (1./4.)*A;
                if (cot(*index_bc) >= 0) {
                    M_triplets.push_back(Eigen::Triplet<double>(ijk[0],ijk[0],(1./8.)*A));
                } else {
                    M_triplets.push_back(Eigen::Triplet<double>(ijk[0],ijk[0],(1./4.)*A));
                }
            }
            index_ab++;
            index_bc++;
        } while (std::next_permutation(ijk.begin(),ijk.end()));
    }
    L.setFromTriplets(L_tripets.begin(),L_tripets.end());
    M.setFromTriplets(M_triplets.begin(),M_triplets.end());
    return std::pair<Eigen::SparseMatrix<double>, Eigen::SparseMatrix<double>>(L,M);
};

Eigen::MatrixXd bbw(cv::Mat image, cv::Mat roi, int m) {
    image.convertTo(image, CV_32F);
    roi.convertTo(roi, CV_32F);
    image /= 255;
    roi /= 255;
    cv::Mat image_LAB;
    cv::cvtColor(image, image_LAB, CV_BGR2Lab);
    std::vector<Eigen::MatrixXd> w(m);
    //First calculate magnitude of gradient of image
    cv::Mat grad_x, grad_y, grad_mag;
    cv::Sobel(image_LAB, grad_x, CV_32F, 1, 0);
    cv::Sobel(image_LAB, grad_y, CV_32F, 0, 1);
    cv::magnitude(grad_x, grad_y, grad_mag);
    double maxVal;
    cv::minMaxLoc(grad_mag, nullptr, &maxVal, nullptr, nullptr);
    grad_mag /= maxVal;
    //Then calculate gradient of roi==0
    cv::Mat roi_mask, roi_grad;
    roi_mask = roi == 0;
    cv::Sobel(roi_mask, grad_x, CV_32F, 1, 0);
    cv::Sobel(roi_mask, grad_y, CV_32F, 0, 1);
    cv::magnitude(grad_x, grad_y, roi_grad);
    cv::minMaxLoc(roi_grad, nullptr, &maxVal, nullptr, nullptr);
    roi_grad /= maxVal;



    cv::Mat G, G_temp;
    cv::cvtColor(grad_mag, grad_mag, CV_BGR2GRAY);
    G = grad_mag + roi_grad;
    for (int j=0; j<5; j++) {
        cv::GaussianBlur(G, G_temp, cv::Size(27,27), 9, 9);
        G += G_temp;
    }
    cv::minMaxLoc(G, nullptr, &maxVal, nullptr, nullptr);
    G /= maxVal;
    std::cout << maxVal << std::endl;

    cv::Mat C(roi.cols, roi.rows, CV_32F);
    roi.convertTo(roi, CV_32F);
    C = roi.mul(1-G);
    //std::cout << roi << std::endl;
    std::vector<cv::Point> H;
    for (int j=0; j<m; j++) {
        cv::Point maxLoc;
        cv::minMaxLoc(C, nullptr, nullptr, nullptr, &maxLoc);
        H.push_back(maxLoc);
        std::cout << maxLoc << std::endl;


        /*
        cv::Mat i;
        double maxVal;
        cv::minMaxLoc(C, nullptr, &maxVal, nullptr, nullptr);
        cv::divide(C, maxVal/255., i);
        i.convertTo(i, CV_8U);
        cv::namedWindow("Display Image", cv::WINDOW_AUTOSIZE );
        cv::imshow("Display Image", i);
        std::cout << G << std::endl;
        cv::waitKey(0);
         */


        int sigma = std::sqrt(cv::norm(roi > 0, cv::NORM_L2) / (CV_PI*m)) ; //fix sigma here
        std::cout << sigma << std::endl;
        //sigma = 5;
        //std::cout << gaussian(C.size(), maxLoc, sigma) << std::endl;
        /*std::cout << maxLoc << std::endl;
        maxLoc -= cv::Point((int)sigma,(int)sigma);
        //maxLoc.x = maxLoc.x ? maxLoc.x >= 0 : 0;
        //maxLoc.y = maxLoc.y ? maxLoc.y >= 0 : 0;
        std::cout << maxLoc << std::endl;
        cv::Mat gauss_kernel = cv::getGaussianKernel(2*sigma, sigma, CV_32F);
        gauss_kernel.copyTo(Gauss_kernel(cv::Rect(maxLoc, cv::Size(2*sigma, 2*sigma))));
        C -= Gauss_kernel;
         */
        C -= gaussian(C.size(), maxLoc, sigma);

        //cv::namedWindow("Display Image", cv::WINDOW_AUTOSIZE );
        //cv::imshow("Display Image", C);
        //cv::waitKey(0);

        cv::circle(image, maxLoc, 3, cv::Scalar(0,255,255),-1);
    }
    /*
    cv::namedWindow("Display Image", cv::WINDOW_AUTOSIZE );
    cv::namedWindow("ROI", cv::WINDOW_AUTOSIZE );
    cv::imshow("Display Image", image);
    cv::imshow("ROI", roi);

    cv::waitKey(0);
     */

    for (auto i=H.begin(); i!=H.end(); i++) {
        std::cout << *i << std::endl;
    }
    Eigen::MatrixXi roi_matrix(roi.rows,roi.cols), x_coords(roi.rows,roi.cols), y_coords(roi.rows,roi.cols);
    for (int i=0; i<roi.rows; i++) {
        y_coords.row(i).array() += i;
    }
    for (int i=0; i<roi.cols; i++) {
        x_coords.col(i).array() += i;
    }
    cv::cv2eigen(roi>0, roi_matrix);
    roi_matrix /= 255;
    Eigen::Map<Eigen::VectorXi> roi_vec(roi_matrix.data(), roi_matrix.size());
    Eigen::Map<Eigen::VectorXi> x_vec(x_coords.data(), x_coords.size());
    Eigen::Map<Eigen::VectorXi> y_vec(y_coords.data(), y_coords.size());
    Eigen::VectorXi scalar_field(roi_vec.size());
    Eigen::MatrixXi coords(roi_matrix.size(), 3), V;
    scalar_field << roi_vec;
    coords.col(0) << x_vec;
    coords.col(1) << y_vec;
    coords.col(2) << Eigen::VectorXi::Zero(roi_matrix.size());


    //HERE DO THE MESH TRIANGULATION
    std::vector<Eigen::RowVector3i> faces;
    Eigen::Matrix2i small(2,2);
    int index[3];
    for (int j=0; j<roi.cols-1; j++) {
        for (int i=0; i<roi.rows-1; i++) {
            small = roi_matrix.block(i,j,2,2);
            switch(small.sum()) {
                case 3:
                    if (small(0,0) == 0) {
                        index[0] = (j+1)*roi_matrix.rows() + i;
                        index[1] = j*roi_matrix.rows() + (i+1);
                        index[2] = (j+1)*roi_matrix.rows() + (i+1);
                        faces.push_back(Eigen::RowVector3i(index[0],index[1],index[2]));
                    } else if (small(1,0) == 0) {
                        index[0] = j*roi_matrix.rows() + i;
                        index[1] = j*roi_matrix.rows() + (i+1);
                        index[2] = (j+1)*roi_matrix.rows() + (i+1);
                        faces.push_back(Eigen::RowVector3i(index[0],index[1],index[2]));
                    } else if (small(1,0) == 0) {
                        index[0] = (j+1)*roi_matrix.rows() + i;
                        index[1] = j*roi_matrix.rows() + i;
                        index[2] = (j+1)*roi_matrix.rows() + (i+1);
                        faces.push_back(Eigen::RowVector3i(index[0],index[1],index[2]));
                    } else if (small(1,0) == 0) {
                        index[0] = j*roi_matrix.rows() + i;
                        index[1] = j*roi_matrix.rows() + (i+1);
                        index[2] = (j+1)*roi_matrix.rows() + i;
                        faces.push_back(Eigen::RowVector3i(index[0],index[1],index[2]));
                    } else {
                        std::cout << "SOMETHING IS GOING WRONG WITH MESH GENERATION" << std::endl;
                    }
                    break;
                case 4:
                    index[0] = j*roi_matrix.rows() + i;
                    index[1] = j*roi_matrix.rows() + (i+1);
                    index[2] = (j+1)*roi_matrix.rows() + i;
                    faces.push_back(Eigen::RowVector3i(index[0],index[1],index[2]));
                    index[0] = (j+1)*roi_matrix.rows() + (i+1);
                    faces.push_back(Eigen::RowVector3i(index[2],index[1],index[0]));
                    break;
                default:
                    break;
            }

        }
    }

    Eigen::MatrixXi F(faces.size(), 3);
    for (int i=0; i<faces.size(); i++) {
        F.row(i) << faces[i];
        Eigen::Vector3d a,b;
        a = coords.row(faces[i](0)).cast<double>() - coords.row(faces[i](1)).cast<double>();
        b = coords.row(faces[i](2)).cast<double>() - coords.row(faces[i](1)).cast<double>();
        coords.row(faces[i](0));
        coords.row(faces[i](1));
        coords.row(faces[i](2));
        //std::cout << 0.5*(a.cross(b).norm()) << std::endl;
    }


    std::vector<cv::Mat> channels;
    Eigen::MatrixXi L, A, B;
    cv::split(image_LAB, channels);
    cv::cv2eigen(channels[0], L);
    cv::cv2eigen(channels[1], A);
    cv::cv2eigen(channels[2], B);
    Eigen::Map<Eigen::VectorXi> L_vec(L.data(),L.size());
    Eigen::Map<Eigen::VectorXi> A_vec(A.data(),A.size());
    Eigen::Map<Eigen::VectorXi> B_vec(B.data(),B.size());
    coords.conservativeResize(coords.rows(), 5);
    coords.col(2) << 16 * L_vec;
    coords.col(3) << 16 * A_vec;
    coords.col(4) << 16 * B_vec;

    auto lm = LM(coords.cast<double>(),F);
    Eigen::SparseMatrix<double> Q;
    Q = lm.first * lm.second.diagonal().asDiagonal().inverse() * lm.first;
    //igl::opengl::glfw::Viewer viewer;
    //viewer.data().set_mesh(coords.cast<double>(), F);
    //viewer.core.align_camera_center(coords.cast<double>(), F);
    //viewer.launch();
    //igl::writeOFF("file.off",coords,F);


    //igl::copyleft::marching_cubes(scalar_field, coords, roi.cols, roi.rows, 4, V, F);
    //igl::writeOFF("file.off", V, F);
    /*
    Eigen::MatrixXd export_coords;
    std::vector<int> c;
    for (int i=0; i<scalar_field.size(); i++) {
        if (scalar_field(i) == 1) {
            c.push_back(i);
        }
    }
    Eigen::Map<Eigen::VectorXi> cc(c.data(), c.size());
    igl::slice(coords, cc, 1, export_coords);
     */
    /*
    for (int i=0; i<scalar_field.size(); i++) {
        if (scalar_field(i) == 1)
            std::cout << "Row " << coords(i,1) << " Column " << coords(i,0) << " Value " << scalar_field(i) << std::endl;
    }*/
    //std::cout << V << std::endl;
    //igl::writeOFF("file.off",V,F);
    //igl::triangle::triangulate();
    /*
    cv::Mat i;
    double maxVal;
    cv::minMaxLoc(G, nullptr, &maxVal, nullptr, nullptr);
    cv::divide(G, maxVal/255., i);
    i.convertTo(i, CV_8U);
    cv::namedWindow("Display Image", cv::WINDOW_AUTOSIZE );
    cv::imshow("Display Image", i);

    cv::waitKey(0);
     */

    return Eigen::MatrixXd(0,0);
}

cv::Mat gaussian(cv::Size size, cv::Point center, double sigma) {
    cv::Mat gauss(size, CV_32FC1);
    for (int i=0; i<gauss.rows; i++) {
        gauss.row(i) = std::exp(-0.5*std::pow((center.y-i)/sigma, 2));
    }
    for (int i=0; i<gauss.cols; i++) {
        gauss.col(i) *= std::exp(-0.5*std::pow((center.x-i)/sigma, 2));
    }
    //gauss *= 1./(2*CV_PI*sigma*sigma);
    return gauss;
}

Eigen::MatrixXd transformations(cv::Mat image_s, cv::Mat image_t, cv::Mat roi, Eigen::MatrixXd w) {
    assert(roi.channels() == 1);

    cv::SiftFeatureDetector detector;
    std::vector<cv::KeyPoint> kp_s, kp_t;
    detector.detect(image_s, kp_s, roi>0);
    detector.detect(image_t, kp_t);
    cv::BFMatcher matcher(cv::NORM_L2);
    cv::SiftDescriptorExtractor extractor;
    cv::Mat d_s, d_t;
    extractor.compute(image_s, kp_s, d_s);
    extractor.compute(image_t, kp_t, d_t);
    std::vector<std::vector<cv::DMatch>> matches;
    std::vector<cv::DMatch> good_matches;
    std::vector<cv::Point2d> M_s, M_t;
    matcher.knnMatch(d_s, d_t, matches, 2);
    Eigen::MatrixXi matches_duplicate = Eigen::MatrixXi::Zero(image_s.rows, image_s.cols);
    matches_duplicate.array() -= 1;
    for (auto i=matches.begin(); i!=matches.end(); i++) {
        cv::DMatch a,b;
        a = i->at(0);
        b = i->at(1);
        if (a.distance < 0.75*b.distance) {
            cv::Point s, t;
            s = kp_s[a.queryIdx].pt;
            t = kp_t[a.trainIdx].pt;
            if (matches_duplicate(s.y, s.x) >= 0) {
                if (a.distance < good_matches[matches_duplicate(s.y, s.x)].distance) {
                    M_s[matches_duplicate(s.y,s.x)] = s;
                    M_t[matches_duplicate(s.y,s.x)] = t;
                    good_matches[matches_duplicate(s.y,s.x)] = a;
                }
            } else {
                M_s.push_back(s);
                M_t.push_back(t);
                matches_duplicate(s.y, s.x) = (int)good_matches.size();
                good_matches.push_back(a);
            }
        }
    }

    typedef Eigen::MatrixXd::Scalar Scalar;

    //auto orient2D = [] (Eigen::Scalar pa[2], Eigen::Scalar pb[2], Eigen::Scalar pc[2]) {
    auto orient2D = [] (const Scalar pa[2], const Scalar pb[2], const Scalar pc[2]) {
        std::cout << pa[0] << "\t" << pa[1] << std::endl;
        std::cout << pb[0] << "\t" << pb[1] << std::endl;
        std::cout << pc[0] << "\t" << pc[1] << std::endl;
        std::cout << std::endl;
        Eigen::Vector3d a,b;
        a << pa[0] - pb[0], pa[1] - pb[1], 0;
        b << pc[0] - pb[0], pc[1] - pb[1], 0;
        double c = a.cross(b)(2);
        if (c> 0) {
            return 1;
        } else if (c< 0) {
            return -1;
        } else {
            return 0;
        }
    };

    //auto incircle = [] (Eigen::Scalar pa[2], Eigen::Scalar pb[2], Eigen::Scalar pc[2], Eigen::Scalar pd[2]) {
    auto incircle = [] (const Scalar pa[2], const Scalar pb[2], const Scalar pc[2], const Scalar pd[2]) {
        Eigen::Matrix4d mat;
        mat.col(0) << std::sqrt(pd[0]*pd[0]+pd[1]*pd[1]), std::sqrt(pa[0]*pa[0]+pa[1]*pa[1]),std::sqrt(pb[0]*pb[0]+pb[1]*pb[1]),std::sqrt(pc[0]*pc[0]+pc[1]*pc[1]);
        mat.col(1) << pd[0], pa[0], pb[0], pc[0];
        mat.col(2) << pd[1], pa[1], pb[1], pc[1];
        mat.col(3) << 1,1,1,1;
        double d = mat.determinant();
        if (d > 0) {
            return -1;
        } else if (d < 0) {
            return 1;
        } else {
            return 0;
        }
    };

    Eigen::MatrixXd V_S(M_s.size(),2), V_T(M_t.size(),2);
    assert(V_S.rows() == V_T.rows());
    for (int i=0; i<V_S.rows(); i++) {
        V_S.row(i) << M_s[i].x, M_s[i].y;
        V_T.row(i) << M_t[i].x, M_t[i].y;
    }
    //std::cout << "V_S: " << V_S << std::endl;
    //std::cout << "V_T: " << V_T << std::endl;
    //TODO FIX DELAUNAY TRIANGULATION FOR DUPLICATE POINTS
    //igl::delaunay_triangulation(V_S, orient2D, incircle, F);
    //std::cout << F << std::endl;
    //Eigen::MatrixXd V_(V_S.rows(),3);
    //V_.block(0,0,V_S.rows(),2) = V_S;
    //igl::writeOFF("/home/parallels/Desktop/Parallels Shared Folders/Downloads/file.off", V_, F);
    Eigen::MatrixXi F(65,3);
    F << 24, 19, 12,
            19, 18, 12,
            15, 16, 14,
            19, 17, 18,
            34, 33, 35,
            30, 21, 12,
            9, 30, 10,
            18, 13, 12,
            13, 30, 12,
            30, 13, 10,
            37, 17, 19,
            17, 37, 15,
            15, 37, 16,
            37, 19, 24,
            23, 34, 35,
            1, 3, 25,
            33, 28, 35,
            28, 22, 35,
            22, 28, 4,
            3, 20, 25,
            20, 32, 25,
            32, 31, 4,
            31, 20, 3,
            20, 31, 32,
            11, 9, 10,
            11, 23, 35,
            13, 11, 10,
            11, 13, 18,
            23, 11, 18,
            30, 29, 21,
            9, 29, 30,
            23, 7, 34,
            17, 7, 18,
            7, 23, 18,
            7, 15, 14,
            7, 17, 15,
            21, 2, 0,
            2, 1, 0,
            27, 32, 4,
            28, 27, 4,
            32, 27, 33,
            27, 28, 33,
            5, 29, 9,
            11, 5, 9,
            5, 26, 21,
            29, 5, 21,
            5, 11, 35,
            26, 5, 35,
            7, 6, 34,
            34, 6, 33,
            6, 32, 33,
            6, 7, 14,
            25, 6, 14,
            32, 6, 25,
            36, 2, 21,
            36, 22, 4,
            31, 36, 4,
            36, 31, 3,
            1, 36, 3,
            2, 36, 1,
            26, 8, 21,
            8, 36, 21,
            36, 8, 22,
            22, 8, 35,
            8, 26, 35;
    /*
    cv::Mat image_matches;
    cv::drawMatches(image_s, kp_s, image_t, kp_t, good_matches, image_matches);

    cv::namedWindow("Display Image", cv::WINDOW_AUTOSIZE );
    cv::imshow("Display Image", image_matches);

    cv::waitKey(0);
     */
    std::vector<int> valid_triangles;
    for (int i=0; i<F.rows(); i++) {
        Eigen::RowVector3d a,b;
        a << V_T.row(F(i,0)) - V_T.row(F(i,1)) , 0;
        b << V_T.row(F(i,2)) - V_T.row(F(i,1)) , 0;
        if (a.cross(b)(2) > 0) {
            valid_triangles.push_back(i);
        }
    }
    Eigen::Map<Eigen::VectorXi> valid_t(valid_triangles.data(), valid_triangles.size());
    Eigen::MatrixXi F_valid;
    igl::slice(F, valid_t, 1, F_valid);

    std::cout << F_valid << std::endl;
    std::vector<Eigen::MatrixXd> piecewise_affine;
    for (auto i=0; i<F_valid.rows(); i++) {
        Eigen::MatrixXd t(3,3), t_(2,3);
        Eigen::VectorXd a(3), b(2);
        for (int j=0; j<3; j++) {
            a << V_S(F_valid(i,j),0), V_S(F_valid(i,j),1), 1;
            b << V_T(F_valid(i,j),0), V_T(F_valid(i,j),1);
            t.col(j) = a;
            t_.col(j) = b;
        }
        piecewise_affine.push_back(t_ * t.inverse());
    }

    cv::Mat A;
    A = cv::findHomography(M_s, M_t, CV_RANSAC);
    std::cout << A << std::endl;


}
