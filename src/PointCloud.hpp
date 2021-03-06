#pragma once

#include <mpi.h>

#include <vector>
#include <iostream>
#include <fstream>
#include <cassert>
#include <type_traits>
#include <memory>
#include <cstddef>


// code location prefix. When compiling as CUDA, makes available on host and device
#ifdef __CUDACC__
#define POINT_LOC_PREFIX __host__ __device__
#else
#define POINT_LOC_PREFIX
#endif

struct Point {
    // Be warned, x, y, z are not initialized to 0 with default constructor
    POINT_LOC_PREFIX Point() {};
    POINT_LOC_PREFIX Point(float x, float y, float z) : x(x), y(y), z(z) {}

    // members
    float x, y, z;

    // the square distance between two points
    POINT_LOC_PREFIX static float distance2(const Point& p1, const Point& p2) {
        float x_diff = p1.x - p2.x;
        float y_diff = p1.y - p2.y;
        float z_diff = p1.z - p2.z;
        return x_diff*x_diff + y_diff*y_diff + z_diff*z_diff;
    }
    
    POINT_LOC_PREFIX float dot(const Point& other) const {
        return x*other.x + y*other.y + z*other.z;
    }

    POINT_LOC_PREFIX Point& operator+=(const Point& rhs) {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        return *this;
    }

    POINT_LOC_PREFIX friend Point operator+(Point lhs, const Point& rhs) {lhs += rhs; return lhs;}

    POINT_LOC_PREFIX Point& operator/=(const float rhs) {
        x /= rhs;
        y /= rhs;
        z /= rhs;
        return *this;
    }

    POINT_LOC_PREFIX Point& operator-=(const Point& rhs) {
        x -= rhs.x;
        y -= rhs.y;
        z -= rhs.z;
        return *this;
    }

    POINT_LOC_PREFIX friend Point operator-(Point lhs, const Point& rhs) {lhs -= rhs; return lhs;}

    
    static MPI_Datatype getMpiDatatype() {
        static_assert(std::is_standard_layout<Point>::value, "Point is not standard layout");

        static bool mpi_datatype_initialized = 0;
        static MPI_Datatype mpi_datatype;

        if (!mpi_datatype_initialized) {
            constexpr int n_elems = 3;
            const int block_lengths[n_elems] = {1, 1, 1};
            const MPI_Aint displacements[n_elems] = {offsetof(Point, x),
                                                     offsetof(Point, y),
                                                     offsetof(Point, z),
                                                    };
            const MPI_Datatype types[n_elems] = {MPI_FLOAT,
                                                 MPI_FLOAT,
                                                 MPI_FLOAT};
            MPI_Datatype struct_type;
            MPI_Type_create_struct(
                n_elems,
                block_lengths,
                displacements,
                types,
                &struct_type
            );

            // account for potential padding in struct
            MPI_Aint lb, extent;
            MPI_Type_get_extent(struct_type, &lb, &extent);
            MPI_Type_create_resized(struct_type, lb, extent, &mpi_datatype);

            // save datatype
            MPI_Type_commit(&mpi_datatype);
            mpi_datatype_initialized = 1;
        }
        return mpi_datatype;
    }
};

template <typename T>
class PointCloud {
public:
    PointCloud() {};
    // PointCloud(const std::vector<Point>& points, const std::vector<Point>& normals);
    PointCloud(const std::shared_ptr<const std::vector<Point>>& points_ptr, const std::vector<Point>& normals);
    std::vector<T> x_vals;
    std::vector<T> y_vals;
    std::vector<T> z_vals;

    std::vector<T> x_normals;
    std::vector<T> y_normals;
    std::vector<T> z_normals;

    bool saveAsPly(std::string filename);
    void setNormals(std::vector<Point> normals);
private:
};


template <typename T>
PointCloud<T>::PointCloud(const std::shared_ptr<const std::vector<Point>>& points_ptr, const std::vector<Point>& normals) {
    const auto& points = *points_ptr;
    assert(points.size() == normals.size());

    size_t n = points.size();

    x_vals.resize(n);
    y_vals.resize(n);
    z_vals.resize(n);

    x_normals.resize(n);
    y_normals.resize(n);
    z_normals.resize(n);

    for (size_t i = 0; i < n; ++i) {
        x_vals[i] = points[i].x;
        y_vals[i] = points[i].y;
        z_vals[i] = points[i].z;

        x_normals[i] = normals[i].x;
        y_normals[i] = normals[i].y;
        z_normals[i] = normals[i].z;
    }
}

template <typename T>
bool PointCloud<T>::saveAsPly(std::string filename) {
    assert(x_vals.size() == y_vals.size());
    assert(x_vals.size() == z_vals.size());

    bool has_normals = x_normals.size() != 0;
    if (has_normals) {
        assert(x_vals.size() == x_normals.size());
        assert(x_vals.size() == y_normals.size());
        assert(x_vals.size() == z_normals.size());
    }

    std::ofstream of;
    of.open(filename);
    if (!of) { return false; }

    of << "ply" << std::endl;
    of << "format ascii 1.0" << std::endl;

    of << "element vertex " << x_vals.size() << std::endl;
    of << "property float x" << std::endl;
    of << "property float y" << std::endl;
    of << "property float z" << std::endl;
    if (has_normals) {
        of << "property float nx" << std::endl;
        of << "property float ny" << std::endl;
        of << "property float nz" << std::endl;
    }
    of << "end_header" << std::endl;
    for (size_t i = 0; i < x_vals.size(); ++i) {
        of << x_vals[i] << " " << y_vals[i] << " " << z_vals[i];
        if (has_normals) {
            of << " " << x_normals[i] << " " << y_normals[i] << " " << z_normals[i];
        }
        of << std::endl;
    }

    of.close();
    return true;
}

template <>
void PointCloud<float>::setNormals(std::vector<Point> normals);

template <typename T>
void PointCloud<T>::setNormals(std::vector<Point> normals) {
    assert(false); // not implemented for non-floats, since Point object holds floats
}
