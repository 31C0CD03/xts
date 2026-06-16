// Copyright (c) 2026, 31C0CD03
// All rights reserved.

#include <cassert>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <vector>

#if defined( __ARM_NEON ) || defined( __ARM_NEON__ )
	#include <arm_neon.h>
#elif defined( __AVX2__ ) || defined( __AVX512F__ )
	#include <immintrin.h>
#endif

namespace xts
{
	static std::ptrdiff_t scan_impl( const std::uint8_t* data, const std::size_t data_len, const std::uint8_t* pattern, const std::size_t pattern_len, const std::uint8_t* mask );

	class pattern
	{
	   public:
		std::vector<std::uint8_t> m_pattern;
		std::vector<std::uint8_t> m_mask;

	   public:
		pattern( std::vector<std::uint8_t> pattern, std::vector<std::uint8_t> mask ) : m_pattern( std::move( pattern ) ), m_mask( std::move( mask ) )
		{
			if ( m_mask.size() != m_pattern.size() )
			{
				throw std::invalid_argument{ "mask.size() must be equal to pattern.size()" };
			}
		}

		std::ptrdiff_t find( const std::uint8_t* data, const std::size_t data_len )
		{
			if ( data == nullptr )
			{
				throw std::invalid_argument{ "data must not be null" };
			}
			else if ( data_len < m_pattern.size() )
			{
				throw std::invalid_argument{ "data must not be shorter than pattern" };
			}

			return scan_impl( data, data_len, m_pattern.data(), m_pattern.size(), m_mask.data() );
		}
	};

#if defined( __ARM_NEON ) || defined( __ARM_NEON__ )

	// NEON accelerated pattern scanner
	//
	static std::ptrdiff_t scan_impl( const std::uint8_t* data, const std::size_t data_len, const std::uint8_t* pattern, const std::size_t pattern_len, const std::uint8_t* mask )
	{
		// Data must not be shorter than the pattern
		//
		assert( data_len >= pattern_len );

		// Valid slices are (0 .. (pattern_len - 1)) through ((data_len - pattern_len) .. (data_len - 1))
		//
		for ( std::size_t i = 0; i <= ( data_len - pattern_len ); i++ )
		{
			int match = 1;

			// Compare 16 bytes of the pattern at a time
			//
			std::size_t j = 0;
			for ( ; j < ( pattern_len & ~15 ); j += 16 )
			{
				const uint8x16_t data_vec    = vld1q_u8( &data[ i + j ] );
				const uint8x16_t pattern_vec = vld1q_u8( &pattern[ j ] );
				const uint8x16_t mask_vec    = vld1q_u8( &mask[ j ] );

				// Compute the diff, mask wildcard bytes to 0
				//
				uint64x2_t diff_vec = vreinterpretq_u64_u8( vandq_u8( veorq_u8( data_vec, pattern_vec ), mask_vec ) );

				// If all bytes are 0 then continue matching
				//
				match &= ( vgetq_lane_u64( diff_vec, 0 ) | vgetq_lane_u64( diff_vec, 1 ) ) == 0;
				if ( !match )
					break;
			}

			// Compare straggler bytes individually if in the middle of a match
			//
			for ( ; match && ( j < pattern_len ); j++ )
				if ( !( match &= ( ( data[ i + j ] ^ pattern[ j ] ) & mask[ j ] ) == 0 ) )
					break;

			if ( match )
				return i;
		}

		return -1;
	}

#elif defined( __AVX512F__ )

	// AVX512 accelerated pattern scanner
	//
	static std::ptrdiff_t scan_impl( const std::uint8_t* data, const std::size_t data_len, const std::uint8_t* pattern, const std::size_t pattern_len, const std::uint8_t* mask )
	{
		// Data must not be shorter than the pattern
		//
		assert( data_len >= pattern_len );

		// Detect alignment for pattern and mask,
		// Data indexed by incrementing offset thus not eligible
		//
		const bool pattern_aligned = ( reinterpret_cast<std::uintptr_t>( pattern ) & 63 ) == 0;
		const bool mask_aligned    = ( reinterpret_cast<std::uintptr_t>( mask ) & 63 ) == 0;

		// Valid slices are (0 .. (pattern_len - 1)) through ((data_len - pattern_len) .. (data_len - 1))
		//
		for ( std::size_t i = 0; i <= ( data_len - pattern_len ); i++ )
		{
			int match = 1;

			// Compare 64 bytes of the pattern at a time
			//
			std::size_t j = 0;
			for ( ; j < ( pattern_len & ~63 ); j += 64 )
			{
				// Detect alignment
				//
				__m512i data_vec    = _mm512_loadu_si512( reinterpret_cast<const __m512i*>( &data[ i + j ] ) );
				__m512i pattern_vec = pattern_aligned ? _mm512_load_si512( reinterpret_cast<const __m512i*>( &pattern[ j ] ) ) : _mm512_loadu_si512( reinterpret_cast<const __m512i*>( &pattern[ j ] ) );
				__m512i mask_vec    = mask_aligned ? _mm512_load_si512( reinterpret_cast<const __m512i*>( &mask[ j ] ) ) : _mm512_loadu_si512( reinterpret_cast<const __m512i*>( &mask[ j ] ) );

				// Compute the diff, mask wildcard bytes to 0
				//
				const __m512i diff_vec = _mm512_and_si512( _mm512_xor_si512( data_vec, pattern_vec ), mask_vec );

				// If all bytes are 0 then continue matching
				//
				match &= _mm512_test_epi64_mask( diff_vec, _mm512_setzero_si512() ) == 0;
				if ( !match )
					break;
			}

			// Compare straggler bytes individually if in the middle of a match
			//
			for ( ; match && ( j < pattern_len ); j++ )
				if ( !( match &= ( ( data[ i + j ] ^ pattern[ j ] ) & mask[ j ] ) == 0 ) )
					break;

			if ( match )
				return i;
		}

		return -1;
	}

#elif defined( __AVX2__ )

	// AVX2 accelerated pattern scanner
	//
	static std::ptrdiff_t scan_impl( const std::uint8_t* data, const std::size_t data_len, const std::uint8_t* pattern, const std::size_t pattern_len, const std::uint8_t* mask )
	{
		// Data must not be shorter than the pattern
		//
		assert( data_len >= pattern_len );

		// Detect alignment for pattern and mask,
		// Data indexed by incrementing offset thus not eligible
		//
		const bool pattern_aligned = ( reinterpret_cast<std::uintptr_t>( pattern ) & 31 ) == 0;
		const bool mask_aligned    = ( reinterpret_cast<std::uintptr_t>( mask ) & 31 ) == 0;

		// Valid slices are (0 .. (pattern_len - 1)) through ((data_len - pattern_len) .. (data_len - 1))
		//
		for ( std::size_t i = 0; i <= ( data_len - pattern_len ); i++ )
		{
			int match = 1;

			// Compare 32 bytes of the pattern at a time
			//
			std::size_t j = 0;
			for ( ; j < ( pattern_len & ~31 ); j += 32 )
			{
				// Detect alignment
				//
				__m256i data_vec    = _mm256_loadu_si256( reinterpret_cast<const __m256i*>( &data[ i + j ] ) );
				__m256i pattern_vec = pattern_aligned ? _mm256_load_si256( reinterpret_cast<const __m256i*>( &pattern[ j ] ) ) : _mm256_loadu_si256( reinterpret_cast<const __m256i*>( &pattern[ j ] ) );
				__m256i mask_vec    = mask_aligned ? _mm256_load_si256( reinterpret_cast<const __m256i*>( &mask[ j ] ) ) : _mm256_loadu_si256( reinterpret_cast<const __m256i*>( &mask[ j ] ) );

				// Compute the diff, mask wildcard bytes to 0
				//
				const __m256i diff_vec = _mm256_and_si256( _mm256_xor_si256( data_vec, pattern_vec ), mask_vec );

				// If all bytes are 0 then continue matching
				//
				match &= _mm256_testz_si256( diff_vec, diff_vec );
				if ( !match )
					break;
			}

			// Compare straggler bytes individually if in the middle of a match
			//
			for ( ; match && ( j < pattern_len ); j++ )
				if ( !( match &= ( ( data[ i + j ] ^ pattern[ j ] ) & mask[ j ] ) == 0 ) )
					break;

			if ( match )
				return i;
		}

		return -1;
	}
#else
	#error No platform acceleration
#endif
}
