#include <stdexcept>
#include <span>
#include <algorithm>

namespace giha {

// common operators for sparse matrix types
namespace sparse {

template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

template <typename T, typename U>
size_t nrow(const SparseVariant<T, U>& a) { return std::visit([](auto const& m) { return m.m; }, a); }

template <typename T, typename U>
size_t ncol(const SparseVariant<T, U>& a) { return std::visit([](auto const& m) { return m.n; }, a); }

// row major
template <typename T, typename U>
inline std::span<U> row(CSRSparse<T, U>& a, int i) {
    return std::span<U>(&a.colIdx[i], a.rowPtr[i+1] - a.rowPtr[i]);
}

template <typename T, typename U>
std::span<U> row(SparseVariant<T, U>& a, int i) {
    return std::visit(overloaded {
        [i](LILSparse<T, U>& m) { return std::span<U>(m.indices.at(static_cast<size_t>(i))); }, 
        [i](ELLSparse<T, U>& m) { return std::span<U>(&m.indices[i], m.width); },
        [i](CSRSparse<T, U>& m) { return row(m, i); },
    }, a);
}

template <typename T, typename U>
std::span<const U> row(const SparseVariant<T, U>& a, int i) {
    return std::visit(overloaded {
        [i](const LILSparse<T, U>& m) { return std::span<const U>(m.indices.at(static_cast<size_t>(i))); }, 
        [i](const ELLSparse<T, U>& m) { return std::span<const U>(&m.indices[i], m.width); },
        [i](const CSRSparse<T, U>& m) { return std::span<const U>(&m.colIdx[i], m.rowPtr[i+1] - m.rowPtr[i]); },
    }, a);
}


template <typename T, typename U>
LILSparse<T, U> transpose(const LILSparse<T, U>& mat) {
    LILSparse<T, U> result(mat.n, mat.m);
    for (size_t i = 0; i < mat.m; i++) {
        for (const auto& col : mat.indices[i]) {                      
            result.indices[col].push_back(static_cast<U>(i));
        }
    }
    return result;
}

template <typename T, typename U>
ELLSparse<T, U> transpose(const ELLSparse<T, U>& mat) {
    ELLSparse<T, U> result(mat.n, mat.m, mat.width);
    for (size_t i = 0; i < mat.m; i++) {
        for (size_t j = 0; j < mat.width; j++) {
            if (mat.indices[i * mat.width + j] != static_cast<U>(-1)) {
                result.indices[mat.indices[i * mat.width + j] * mat.width + i] = static_cast<U>(i);
            }
        }
    }
    return result;
}

template <typename T, typename U>
CSRSparse<T, U> transpose(const CSRSparse<T, U>& mat) {
    // CSR transpose not implemented
    throw std::runtime_error("CSR transpose not implemented");
}

template <typename T, typename U>
SparseVariant<T, U> transpose(const SparseVariant<T, U>& mat) {
    return std::visit([](const auto& m) -> SparseVariant<T, U> { return transpose(m); }, mat);
}

} // namespace sparse


namespace sparse {

template <typename T, typename U>
CSRSparse<T, U> toCSR(const LILSparse<T, U>& lil) {

    CSRSparse<T, U> csr(lil.m, lil.n);

    // fill row ptrs
    for (size_t i = 0; i < lil.m; i++) {
        csr.rowPtr[i + 1]  = csr.rowPtr[i] + static_cast<U>(lil.indices[i].size());
    }

    // fill col ind
    csr.resizeNNZ(csr.rowPtr.back());
    size_t offset = 0;
    for (size_t i = 0; i < lil.m; ++i) {
        const auto& row = lil.indices[i];
        std::copy(row.begin(), row.end(), csr.colIdx.begin() + static_cast<std::ptrdiff_t>(offset));
        offset += row.size();
    }

    if (lil.values) {
        const auto& lilValues = lil.values.value();
        csr.values = std::vector<T>(csr.rowPtr.back());

        size_t valueOffset = 0;
        for (size_t i = 0; i < lil.m; ++i) {
            const auto& rowValues = lilValues[i];
            std::copy(rowValues.begin(), rowValues.end(),
                      csr.values->begin() + static_cast<std::ptrdiff_t>(valueOffset));
            valueOffset += rowValues.size();
        }
    }

    return csr;
}
} // namespace sparse
} // namespace giha
