#pragma once

#include <giha.h>
#include <vector>
#include <optional>

namespace giha {

enum MatrixOrder {
    RowMajor,
    ColMajor,
};

enum SparseFormat {
    LIL,
    ELL,    
    CSR,
};

class SparseMatrixBase {
public:
    virtual ~SparseMatrixBase() = default;
    virtual SparseFormat format() const = 0;
};

template <SparseFormat Format>
class SparseMatrix : public SparseMatrixBase {
public:
    static constexpr SparseFormat format_ = Format;

    SparseMatrix() : m(0), n(0) {}
    SparseMatrix(size_t _m, size_t _n): m(_m), n(_n) {}

    SparseMatrix(const SparseMatrix&) = delete;
    SparseMatrix& operator=(const SparseMatrix&) = delete;

    SparseMatrix(SparseMatrix&&) noexcept = default;
    SparseMatrix& operator=(SparseMatrix&&) noexcept = default;

    size_t nrow() const { return m; }
    size_t ncol() const { return n; }
    SparseFormat format() const override { return format_; }

    size_t m, n;
}; 

template <typename T, typename U>
class LILSparse : public SparseMatrix<LIL> {
public:

    LILSparse(size_t rows = 0, size_t cols = 0, bool initValue = false)
    : SparseMatrix<LIL>(rows, cols), indices(std::vector<std::vector<U>>(rows)) {
        if (initValue) {
            values.emplace(std::vector<std::vector<T>>(rows));
        }
    }

    std::vector<std::vector<U>> indices;
    std::optional<std::vector<std::vector<T>>> values;
};

template <typename T, typename U>
class ELLSparse : public SparseMatrix<ELL> {
public:

    ELLSparse(size_t rows = 0, size_t cols = 0, size_t width = 0) {
        this->m = rows;
        this->n = cols;
        this->width = width;
        indices.resize(rows * width, U(0));
    }

    size_t width = 0; // max non-zeros per row
    std::vector<U> indices;
    std::optional<std::vector<T>> values;
};

template <typename T, typename U>
class CSRSparse : public SparseMatrix<CSR> {
public:
    CSRSparse(size_t rows = 0, size_t cols = 0, size_t nnz = 0)
    : SparseMatrix<CSR>(rows, cols), rowPtr(std::vector<U>(rows + 1, 0)) {}

    void resizeNNZ(size_t _nnz) {
        nnz = _nnz;
        colIdx.resize(nnz);
        // values.resize(nnz);
    }

    size_t nnz;
    std::vector<U> rowPtr;
    std::vector<U> colIdx;
    std::optional<std::vector<T>> values;
}; 

} // namespace giha

#include <variant>

namespace giha {

template <typename T, typename U>
using SparseVariant = std::variant<LILSparse<T, U>, ELLSparse<T, U>, CSRSparse<T, U>>;

} // namespace giha

#include "sparse.ipp"
