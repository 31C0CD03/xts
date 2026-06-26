// Copyright (c) 2026, 31C0CD03
// All rights reserved.

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

#include <xts/algorithm.hpp>

namespace xts
{
	namespace tests
	{
		static std::ptrdiff_t naive_scan( const std::uint8_t* data, std::size_t data_len, const std::uint8_t* pattern, std::size_t pattern_len, const std::uint8_t* mask )
		{
			if ( data_len < pattern_len )
				return -1;
			for ( std::ptrdiff_t i = 0; i <= data_len - pattern_len; ++i )
			{
				bool ok = true;
				for ( std::ptrdiff_t j = 0; j < pattern_len; ++j )
				{
					std::uint8_t m = mask ? mask[ j ] : 0xFF;
					if ( ( ( data[ i + j ] ^ pattern[ j ] ) & m ) != 0 )
					{
						ok = false;
						break;
					}
				}
				if ( ok )
					return i;
			}
			return -1;
		}

		struct test_case
		{
				const char* m_name;
				std::vector<std::uint8_t> m_data;
				std::vector<std::uint8_t> m_pattern;
				std::vector<std::uint8_t> m_mask;
				std::ptrdiff_t m_expected;
		};
	}
}

int main()
{
	auto make_mask = []( std::size_t n, std::uint8_t v ) { return std::vector<std::uint8_t>( n, v ); };

	std::vector<xts::tests::test_case> tests;

	tests.push_back( { "basic", { 1, 2, 3, 4, 5 }, { 3, 4 }, make_mask( 2, 0xFF ), 2 } );
	tests.push_back( { "no-match", { 1, 2, 3, 4, 5 }, { 9, 9 }, make_mask( 2, 0xFF ), -1 } );
	tests.push_back( { "equal", { 7, 8, 9 }, { 7, 8, 9 }, make_mask( 3, 0xFF ), 0 } );
	tests.push_back( { "wildcard-middle", { 1, 2, 3, 4, 5, 6 }, { 3, 0xFF, 5 }, { 0xFF, 0x00, 0xFF }, 2 } );
	tests.push_back( { "multiple-occurrences", { 1, 2, 3, 1, 2, 3 }, { 1, 2, 3 }, make_mask( 3, 0xFF ), 0 } );
	tests.push_back( { "overlap", { 1, 1, 1, 1 }, { 1, 1 }, make_mask( 2, 0xFF ), 0 } );
	tests.push_back( { "empty-pattern", { 1, 2, 3 }, {}, {}, 0 } );
	tests.push_back( { "all-wildcard", { 5, 6, 7 }, { 10, 11, 12 }, make_mask( 3, 0x00 ), 0 } );
	tests.push_back( { "original-sample",
					   { 1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 1, 2, 2, 3, 4, 5, 1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 1, 1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 1, 2, 2, 3, 4, 5, 1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 1, 2, 3, 4, 5, 9 },
					   { 4, 5, 1, 2, 3, 4, 5, 9 },
					   make_mask( 8, 0xFF ),
					   -1 } );

	int passed = 0;
	int failed = 0;
	for ( const auto& t : tests )
	{
		auto p			   = xts::pattern{ t.m_pattern, t.m_mask };
		std::ptrdiff_t res = p.find( t.m_data.data(), t.m_data.size() );

		std::ptrdiff_t expected = t.m_expected;
		if ( expected == -1 )
		{
			expected = xts::tests::naive_scan( t.m_data.empty() ? nullptr : t.m_data.data(), t.m_data.size(), t.m_pattern.empty() ? nullptr : t.m_pattern.data(), t.m_pattern.size(), t.m_mask.empty() ? nullptr : t.m_mask.data() );
		}

		bool ok = ( res == expected );
		if ( ok )
		{
			printf( "PASS: %s -> %zd\n", t.m_name, res );
			++passed;
		}
		else
		{
			printf( "FAIL: %s -> got %zd expected %zd\n", t.m_name, res, expected );
			++failed;
		}
	}

	printf( "\nSummary: %d passed, %d failed\n", passed, failed );
	return failed == 0 ? 0 : 1;
}
