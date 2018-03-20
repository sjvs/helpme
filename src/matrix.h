// BEGINLICENSE
//
// This file is part of helPME, which is distributed under the BSD 3-clause license,
// as described in the LICENSE file in the top level directory of this project.
//
// Author: Andrew C. Simmonett
//
// ENDLICENSE
#ifndef _HELPME_MATRIX_H_
#define _HELPME_MATRIX_H_

#include <algorithm>
#include <complex>
#include <exception>
#include <initializer_list>
#include <iostream>
#include <iomanip>
#include <numeric>
#include <tuple>

#include "lapack_wrapper.h"
#include "memory.h"

namespace helpme {

/*!
 * A helper function to transpose a dense matrix in place, gratuitously stolen from
 * https://stackoverflow.com/questions/9227747/in-place-transposition-of-a-matrix
 */
template <class RandomIterator>
void transposeMemoryInPlace(RandomIterator first, RandomIterator last, int m) {
    const int mn1 = (last - first - 1);
    const int n = (last - first) / m;
    std::vector<bool> visited(last - first);
    RandomIterator cycle = first;
    while (++cycle != last) {
        if (visited[cycle - first]) continue;
        int a = cycle - first;
        do {
            a = a == mn1 ? mn1 : (n * a) % mn1;
            std::swap(*(first + a), *cycle);
            visited[a] = true;
        } while ((first + a) != cycle);
    }
}

/*!
 * \brief The Matrix class is designed to serve as a convenient wrapper to simplify 2D matrix operations.
 *        It assumes dense matrices with contiguious data and the fast running index being the right
 *        (column) index.  The underlying memory may have already been allocated elsewhere by C, Fortran
 *        or Python, and is directly manipulated in place, saving an expensive copy operation.  To provide
 *        read-only access to such memory address, use a const template type.
 */
template <typename Real>
class Matrix {
   protected:
    /// The number of rows in the matrix.
    size_t nRows_;
    /// The number of columns in the matrix.
    size_t nCols_;
    /// A vector to conveniently allocate data, if we really need to.
    helpme::vector<Real> allocatedData_;
    /// Pointer to the raw data, whose allocation may not be controlled by us.
    Real* data_;

   public:
    enum class SortOrder { Ascending, Descending };

    const Real& operator()(int row, int col) const { return *(data_ + row * nCols_ + col); }
    Real& operator()(int row, int col) { return *(data_ + row * nCols_ + col); }
    const Real* operator[](int row) const { return data_ + row * nCols_; }
    Real* operator[](int row) { return data_ + row * nCols_; }

    Real* begin() { return data_; }
    Real* end() { return data_ + nRows_ * nCols_; }
    const Real* cbegin() const { return data_; }
    const Real* cend() const { return data_ + nRows_ * nCols_; }

    /*!
     * \brief The sliceIterator struct provides a read-only view of a sub-block of a matrix, with arbitrary size.
     */
    struct sliceIterator {
        Real *begin_, *end_, *ptr_;
        size_t stride_;
        sliceIterator(Real* start, Real* end, size_t stride) : begin_(start), end_(end), ptr_(start), stride_(stride) {}
        sliceIterator begin() const { return sliceIterator(begin_, end_, stride_); }
        sliceIterator end() const { return sliceIterator(end_, end_, 0); }
        sliceIterator cbegin() const { return sliceIterator(begin_, end_, stride_); }
        sliceIterator cend() const { return sliceIterator(end_, end_, 0); }
        bool operator!=(const sliceIterator& other) { return ptr_ != other.ptr_; }
        sliceIterator operator*=(Real val) {
            for (auto& element : *this) element *= val;
            return *this;
        }
        sliceIterator operator/=(Real val) {
            Real invVal = 1 / val;
            for (auto& element : *this) element *= invVal;
            return *this;
        }
        sliceIterator operator-=(Real val) {
            for (auto& element : *this) element -= val;
            return *this;
        }
        sliceIterator operator+=(Real val) {
            for (auto& element : *this) element += val;
            return *this;
        }
        sliceIterator operator++() {
            ptr_ += stride_;
            return *this;
        }
        const Real& operator[](size_t index) { return *(begin_ + index); }
        size_t size() const { return std::distance(begin_, end_) / stride_; }
        void assertSameSize(const sliceIterator& other) const {
            if (size() != other.size())
                throw std::runtime_error("Slice operations only supported for slices of the same size.");
        }
        void assertContiguous(const sliceIterator& iter) const {
            if (iter.stride_ != 1)
                throw std::runtime_error(
                    "Slice operations called on operation that is only allowed for contiguous data.");
        }
        Matrix<Real> operator-(const sliceIterator& other) const {
            assertSameSize(other);
            assertContiguous(*this);
            assertContiguous(other);
            Matrix ret(1, size());
            std::transform(begin_, end_, other.begin_, ret[0],
                           [](const Real& a, const Real& b) -> Real { return a - b; });
            return ret;
        }
        sliceIterator operator-=(const sliceIterator& other) const {
            assertSameSize(other);
            assertContiguous(*this);
            assertContiguous(other);
            std::transform(begin_, end_, other.begin_, begin_,
                           [](const Real& a, const Real& b) -> Real { return a - b; });
            return *this;
        }
        sliceIterator operator+=(const sliceIterator& other) const {
            assertSameSize(other);
            assertContiguous(*this);
            assertContiguous(other);
            std::transform(begin_, end_, other.begin_, begin_,
                           [](const Real& a, const Real& b) -> Real { return a + b; });
            return *this;
        }
        Real& operator*() { return *ptr_; }
    };

    /*!
     * \brief row returns a read-only iterator over a given row.
     * \param r the row to return.
     * \return the slice in memory corresponding to the rth row.
     */
    sliceIterator row(size_t r) const { return sliceIterator(data_ + r * nCols_, data_ + (r + 1) * nCols_, 1); }

    /*!
     * \brief col returns a read-only iterator over a given column.
     * \param c the column to return.
     * \return the slice in memory corresponding to the cth column.
     */
    sliceIterator col(size_t c) const { return sliceIterator(data_ + c, data_ + nRows_ * nCols_ + c, nCols_); }

    /*!
     * \return the number of rows in this matrix.
     */
    size_t nRows() const { return nRows_; }

    /*!
     * \return the number of columns in this matrix.
     */
    size_t nCols() const { return nCols_; }

    /*!
     * \brief Matrix Constructs an empty matrix.
     */
    Matrix() {}

    /*!
     * \brief Matrix Constructs a new matrix, allocating memory.
     * \param nRows the number of rows in the matrix.
     * \param nCols the number of columns in the matrix.
     */
    Matrix(size_t nRows, size_t nCols)
        : nRows_(nRows), nCols_(nCols), allocatedData_(nRows * nCols, 0), data_(allocatedData_.data()) {}

    /*!
     * \brief Matrix Constructs a new matrix, allocating memory and initializing values using the braced initializer.
     * \param data a braced initializer list of braced initializer lists containing the values to be stored in the
     * matrix.
     */
    Matrix(std::initializer_list<std::initializer_list<Real>> data) {
        nRows_ = data.size();
        nCols_ = nRows_ ? data.begin()->size() : 0;
        allocatedData_.reserve(nRows_ * nCols_);
        for (auto& row : data) {
            if (row.size() != nCols_) throw std::runtime_error("Inconsistent row dimensions in matrix specification.");
            allocatedData_.insert(allocatedData_.end(), row.begin(), row.end());
        }
        data_ = allocatedData_.data();
    }

    /*!
     * \brief Matrix Constructs a new column vector, allocating memory and initializing values using the braced
     * initializer. \param data a braced initializer list of braced initializer lists containing the values to be stored
     * in the matrix.
     */
    Matrix(std::initializer_list<Real> data) : allocatedData_(data), data_(allocatedData_.data()) {
        nRows_ = data.size();
        nCols_ = 1;
    }

    /*!
     * \brief Matrix Constructs a new matrix using already allocated memory.
     * \param ptr the already-allocated memory underlying this matrix.
     * \param nRows the number of rows in the matrix.
     * \param nCols the number of columns in the matrix.
     */
    Matrix(Real* ptr, size_t nRows, size_t nCols) : nRows_(nRows), nCols_(nCols), data_(ptr) {}

    /*!
     * \brief cast make a copy of this matrix, with its elements cast as a different type.
     * \tparam NewReal the type to cast each element to.
     * \return the copy of the matrix with the new type.
     */
    template <typename NewReal>
    Matrix<NewReal> cast() const {
        Matrix<NewReal> newMat(nRows_, nCols_);
        NewReal* newPtr = newMat[0];
        const Real* dataPtr = data_;
        for (size_t addr = 0; addr < nRows_ * nCols_; ++addr) *newPtr++ = static_cast<NewReal>(*dataPtr++);
        return newMat;
    }

    /*!
     * \brief setConstant sets all elements of this matrix to a specified value.
     * \param value the value to set each element to.
     */
    void setConstant(Real value) { std::fill(begin(), end(), value); }

    /*!
     * \brief setZero sets each element of this matrix to zero.
     */
    void setZero() { setConstant(0); }

    /*!
     * \brief isNearZero checks that each element in this matrix has an absolute value below some threshold.
     * \param threshold the value below which an element is considered zero.
     * \return whether all values are near zero or not.
     */
    bool isNearZero(Real threshold = 1e-10f) const {
        return !std::any_of(cbegin(), cend(), [&](const Real& val) { return std::abs(val) > threshold; });
    }

    /*!
     * \brief inverse inverts this matrix, leaving the original matrix untouched.
     * \return the inverse of this matrix.
     */
    Matrix inverse() const {
        assertSquare();

        Matrix matrixInverse(nRows_, nRows_);

        if (nRows() == 3) {
            // 3x3 is a really common case, so treat it here as.
            Real determinant = data_[0] * (data_[4] * data_[8] - data_[7] * data_[5]) -
                               data_[1] * (data_[3] * data_[8] - data_[5] * data_[6]) +
                               data_[2] * (data_[3] * data_[7] - data_[4] * data_[6]);

            Real determinantInverse = 1 / determinant;

            matrixInverse.data_[0] = (data_[4] * data_[8] - data_[7] * data_[5]) * determinantInverse;
            matrixInverse.data_[1] = (data_[2] * data_[7] - data_[1] * data_[8]) * determinantInverse;
            matrixInverse.data_[2] = (data_[1] * data_[5] - data_[2] * data_[4]) * determinantInverse;
            matrixInverse.data_[3] = (data_[5] * data_[6] - data_[3] * data_[8]) * determinantInverse;
            matrixInverse.data_[4] = (data_[0] * data_[8] - data_[2] * data_[6]) * determinantInverse;
            matrixInverse.data_[5] = (data_[3] * data_[2] - data_[0] * data_[5]) * determinantInverse;
            matrixInverse.data_[6] = (data_[3] * data_[7] - data_[6] * data_[4]) * determinantInverse;
            matrixInverse.data_[7] = (data_[6] * data_[1] - data_[0] * data_[7]) * determinantInverse;
            matrixInverse.data_[8] = (data_[0] * data_[4] - data_[3] * data_[1]) * determinantInverse;
        } else {
            // Generic case; just use spectral decomposition, invert the eigenvalues, and stitch back together.
            // Note that this only works for symmetric matrices.  Need to hook into Lapack for a general
            // inversion routine if this becomes a limitation.
            return this->applyOperation([](Real& element) { element = 1 / element; });
        }
        return matrixInverse;
    }

    /*!
     * \brief assertSymmetric checks that this matrix is symmetric within some threshold.
     * \param threshold the value below which an pair's difference is considered zero.
     */
    void assertSymmetric(const Real& threshold = 1e-10f) const {
        assertSquare();
        for (int row = 0; row < nRows_; ++row) {
            for (int col = 0; col < row; ++col) {
                if (std::abs(data_[row * nCols_ + col] - data_[col * nCols_ + row]) > threshold)
                    throw std::runtime_error("Unexpected non-symmetric matrix found.");
            }
        }
    }

    /*!
     * \brief applyOperationToEachElement modifies every element in the matrix by applying an operation.
     * \param function a unary operator describing the operation to perform.
     */
    void applyOperationToEachElement(const std::function<void(Real&)>& function) {
        std::for_each(begin(), end(), function);
    }

    /*!
     * \brief applyOperation applies an operation to this matrix using the spectral decomposition,
     *        leaving the original untouched.  Only for symmetric matrices, as coded.
     * \param function a undary operator describing the operation to perform.
     * \return the matrix transformed by the operator.
     */
    Matrix applyOperation(const std::function<void(Real&)>& function) const {
        assertSymmetric();

        auto eigenPairs = this->diagonalize();
        Matrix evalsReal = std::get<0>(eigenPairs);
        Matrix evalsImag = std::get<1>(eigenPairs);
        Matrix evecs = std::get<2>(eigenPairs);
        if (!evalsImag.isNearZero())
            throw std::runtime_error("Unexpected complex eigenvalues encountered when applying operator to Matrix.");
        evalsReal.applyOperationToEachElement(function);
        Matrix evecsT = evecs.transpose();
        for (int row = 0; row < nRows_; ++row) {
            Real transformedEigenvalue = evalsReal[row][0];
            std::for_each(evecsT.data_ + row * nCols_, evecsT.data_ + (row + 1) * nCols_,
                          [&](Real& val) { val *= transformedEigenvalue; });
        }
        return evecs * evecsT;
    }

    /*!
     * \brief assertSameSize make sure that this Matrix has the same dimensions as another Matrix.
     * \param other the matrix to compare to.
     */
    void assertSameSize(const Matrix& other) const {
        if (nRows_ != other.nRows_ || nCols_ != other.nCols_)
            throw std::runtime_error("Attepting to compare matrices of different sizes!");
    }

    /*!
     * \brief assertSquare make sure that this Matrix is square.
     */
    void assertSquare() const {
        if (nRows_ != nCols_)
            throw std::runtime_error("Attepting to perform a square matrix operation on a non-square matrix!");
    }

    /*!
     * \brief multiply this matrix with another, returning a new matrix containing the product.
     * \param other the right hand side of the matrix product.
     * \return the product of this matrix with the matrix other.
     */
    Matrix multiply(const Matrix& other) const {
        // TODO one fine day this should be replaced by GEMM calls, if matrix multiplies actually get used much.
        if (nCols_ != other.nRows_)
            throw std::runtime_error("Attempting to multiply matrices with incompatible dimensions.");
        Matrix product(nRows_, other.nCols_);
        Real* output = product.data_;
        for (int row = 0; row < nRows_; ++row) {
            const Real* rowPtr = data_ + row * nCols_;
            for (int col = 0; col < other.nCols_; ++col) {
                for (int link = 0; link < nCols_; ++link) {
                    *output += rowPtr[link] * other.data_[link * other.nCols_ + col];
                }
                ++output;
            }
        }
        return product;
    }

    /*!
     * \brief operator * a convenient wrapper for the multiply function.
     * \param other the right hand side of the matrix product.
     * \return the product of this matrix with the matrix other.
     */
    Matrix operator*(const Matrix& other) const { return this->multiply(other); }

    /*!
     * \brief almostEquals checks that two matrices have all elements the same, within some specificied tolerance.
     * \param other the matrix against which we're comparing.
     * \param tol the amount that each element is allowed to deviate by.
     * \return whether the two matrices are almost equal.
     */
    template <typename T = Real, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
    bool almostEquals(const Matrix& other, Real tolerance = 1e-6) const {
        // The floating point version
        assertSameSize(other);

        return std::equal(cbegin(), cend(), other.cbegin(), [&tolerance](Real a, Real b) -> bool {
            return (((a - b) < std::real(tolerance)) && ((a - b) > -std::real(tolerance)));
        });
    }
    template <typename T = Real, typename std::enable_if<!std::is_floating_point<T>::value, int>::type = 0>
    bool almostEquals(const Matrix& other, Real tolerance = 1e-6) const {
        // The complex version
        assertSameSize(other);

        auto tol = std::real(tolerance);
        // This is a little confusing, but the type "Real" is actually some king of std::complex<...>.
        return std::equal(cbegin(), cend(), other.cbegin(), [&tol](Real a, Real b) -> bool {
            return (((a.real() - b.real()) < tol) && ((a.real() - b.real()) > -tol) && ((a.imag() - b.imag()) < tol) &&
                    ((a.imag() - b.imag()) > -tol));
        });
    }

    /*!
     * \brief dot computes the inner product of this matrix with another.
     * \param other the other matrix in the inner product, which must have the same dimensions.
     * \return the inner product.
     */
    Real dot(const Matrix& other) const {
        assertSameSize(other);

        return std::inner_product(cbegin(), cend(), other.cbegin(), Real(0));
    }

    /*!
     * \brief write formatted matrix to a stream object.
     * \param os stream object to write to.
     * \return modified stream object.
     */
    std::ostream& write(std::ostream& os) const {
        for (int row = 0; row < nRows_; ++row) {
            const Real* rowData = data_ + row * nCols_;
            std::cout.setf(std::ios::fixed, std::ios::floatfield);
            for (int col = 0; col < nCols_; ++col) {
                os << std::setw(18) << std::setprecision(10) << rowData[col] << " ";
            }
            os << std::endl;
        }
        os << std::endl;
        return os;
    }

    /*!
     * \brief transposeInPlace transposes this matrix in place.
     */
    void transposeInPlace() {
        transposeMemoryInPlace(begin(), end(), nCols_);
        std::swap(nCols_, nRows_);
    }

    /*!
     * \brief clone make a new copy of this matrix by deep copying the data.
     * \return the copy of this matrix.
     */
    Matrix clone() const {
        Matrix newMatrix = Matrix(nRows_, nCols_);
        std::copy(cbegin(), cend(), newMatrix.begin());
        return newMatrix;
    }

    /*!
     * \brief transpose this matrix, leaving the original untouched.
     * \return a transposed deep copy of this matrix.
     */
    Matrix transpose() const {
        Matrix copy = this->clone();
        copy.transposeInPlace();
        return copy;
    }

    /*!
     * \brief diagonalize diagonalize this matrix, leaving the original untouched.
     * \param order how to order the (eigenvalue,eigenvector) pairs, where the sort key is the real part of the
     * eigenvalue.
     * \return a tuple of <real eigenvalue compnents, imaginary eigenvalue components, eigenvectors> sorted
     * according to the order variable.  The eigenvectors are stored by column.
     */
    std::tuple<Matrix<Real>, Matrix<Real>, Matrix<Real>> diagonalize(SortOrder order = SortOrder::Ascending) const {
        if (nRows_ != nCols_) throw std::runtime_error("Attempting to diagonalize a non-square matrix.");
        Matrix evalsReal(nRows_, 1);
        Matrix evalsImag(nRows_, 1);
        Matrix evecs(nRows_, nRows_);
        std::vector<Real> clone(data_, data_ + nRows_ * nCols_);

        int info;
        Real workDim;
        LapackWrapper<Real>::diagonalizer()('V', 'N', nRows_, clone.data(), nCols_, evalsReal.data_, evalsImag.data_,
                                            evecs.data_, nCols_, nullptr, 1, &workDim, -1, &info);
        int scratchSize = static_cast<int>(workDim);
        std::vector<Real> work(scratchSize);
        LapackWrapper<Real>::diagonalizer()('V', 'N', nRows_, clone.data(), nCols_, evalsReal.data_, evalsImag.data_,
                                            evecs.data_, nCols_, nullptr, 1, work.data(), scratchSize, &info);
        if (info) throw std::runtime_error("Something went wrong during diagonalization!");

        struct eigenInfo {
            Real valueReal, valueImag, *vector;
            eigenInfo(Real r, Real i, Real* v) : valueReal(r), valueImag(i), vector(v) {}
            bool operator<(eigenInfo const& other) const { return valueReal < other.valueReal; }
        };

        std::vector<eigenInfo> eigenTuples;
        for (int val = 0; val < nRows_; ++val)
            eigenTuples.push_back(eigenInfo(evalsReal[val][0], evalsImag[val][0], evecs[val]));

        std::sort(eigenTuples.begin(), eigenTuples.end());
        if (order == SortOrder::Descending) std::reverse(eigenTuples.begin(), eigenTuples.end());
        for (int val = 0; val < nRows_; ++val) {
            const auto& e = eigenTuples[val];
            evalsReal.data_[val] = e.valueReal;
            evalsImag.data_[val] = e.valueImag;
            std::copy(e.vector, e.vector + nCols_, clone.data() + val * nCols_);
        }
        std::copy(clone.begin(), clone.end(), evecs.begin());
        evecs.transposeInPlace();
        return std::make_tuple(std::move(evalsReal), std::move(evalsImag), std::move(evecs));
    }
};

/*!
 * A helper function to allow printing of Matrix objects to a stream.
 */
template <typename Real>
std::ostream& operator<<(std::ostream& os, Matrix<Real> const& m) {
    return m.write(os);
}

}  // Namespace helpme
#endif  // Header guard