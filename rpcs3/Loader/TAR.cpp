#include "stdafx.h"

#include "TAR.h"

#include <cstdlib>

tar_object::tar_object(const fs::file& file) : m_file(file)
{

}

TARHeader tar_object::read_header(u64 offset)
{
	m_file.seek(offset);
	TARHeader header;
	m_file.read(header);
	return header;
}

int octalToDecimal(int octalNumber)
{
	int decimalNumber = 0, i = 0, rem;
	while (octalNumber != 0)
	{
		rem = octalNumber % 10;
		octalNumber /= 10;
		decimalNumber += rem * pow(8, i);
		++i;
	}
	return decimalNumber;
}

std::vector<std::string> tar_object::get_filenames()
{
	std::vector<std::string> vec;
	get_file("");
	for (auto it = m_map.cbegin(); it != m_map.cend(); ++it)
	{
		vec.push_back(it->first);
	}
	return vec;
}

fs::file tar_object::get_file(std::string path)
{
	auto it = m_map.find(path);
	if (it != m_map.end())
	{
		TARHeader header = read_header(it->second);
		int size = octalToDecimal(atoi(header.size));
		std::vector<u8> buf(size);
		m_file.read(buf, size);
		int offset = (m_file.pos() + 512 - 1) & ~(512 - 1);
		m_file.seek(offset);
		return fs::make_stream(std::move(buf));
	}
	else //continue scanning from last file entered
	{
		while (m_file.pos() < m_file.size()) {
			TARHeader header = read_header(largest_offset);
			m_map[header.name] = largest_offset;
			int size = octalToDecimal(atoi(header.size));
			if (path.compare(header.name) == 0) { //path is equal, read file and advance offset to start of next block
				std::vector<u8> buf(size);
				m_file.read(buf, size);
				int offset = (m_file.pos() + 512 - 1) & ~(512 - 1);
				m_file.seek(offset);
				largest_offset = offset;

				return fs::make_stream(std::move(buf));
			}
			else { // just advance offset to next block
				m_file.seek(size, fs::seek_mode::seek_cur);
				int offset = (m_file.pos() + 512 - 1) & ~(512 - 1);
				m_file.seek(offset);
				largest_offset = offset;
			}
		} 
		
		return fs::file();
	}
}