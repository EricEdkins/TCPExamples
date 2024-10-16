#pragma once

#include <vector>

class Buffer
{
public:

	std::vector<uint8_t> m_BufferData;
	int m_WriteIndex;
	int m_ReadIndex;

	Buffer(int size = 512)
	{
		m_BufferData.resize(size);
		m_WriteIndex = 0;
		m_ReadIndex = 0;
	}

	~Buffer() {}

	void GrowIfNeeded(int requiredSize)
	{
		if (m_WriteIndex + requiredSize > m_BufferData.size())
		{
			m_BufferData.resize(m_BufferData.size() + requiredSize);
		}
	}


	void WriteUInt16LE(uint16_t value)
	{
		GrowIfNeeded(2); 
		m_BufferData[m_WriteIndex++] = value & 0xFF;         
		m_BufferData[m_WriteIndex++] = (value >> 8) & 0xFF;  
	}

	
	uint16_t ReadUInt16LE()
	{
		if (m_ReadIndex + 2 > m_WriteIndex)
			throw std::out_of_range("Read past buffer end");

		uint16_t value = 0;
		value |= m_BufferData[m_ReadIndex++];          
		value |= m_BufferData[m_ReadIndex++] << 8;    

		return value;
	}

	void WriteUInt32LE(uint32_t value)
	{
		GrowIfNeeded(4);
		m_BufferData[m_WriteIndex++] = value;
		m_BufferData[m_WriteIndex++] = value >> 8;
		m_BufferData[m_WriteIndex++] = value >> 16;
		m_BufferData[m_WriteIndex++] = value >> 24;
	}

	uint32_t ReadUInt32LE()
	{
		uint32_t value = 0;

		if (m_ReadIndex < m_WriteIndex) {
			std::cout << "Read index out of bounds client side";
		}

		value |= m_BufferData[m_ReadIndex++];
		value |= m_BufferData[m_ReadIndex++] << 8;
		value |= m_BufferData[m_ReadIndex++] << 16;
		value |= m_BufferData[m_ReadIndex++] << 24;

		return value;
	}

	void WriteString(const std::string& str)
	{
		int strLength = str.length();
		for (int i = 0; i < strLength; i++)
		{
			m_BufferData[m_WriteIndex++] = str[i];
		}
	}

	std::string ReadString(uint32_t length)
	{
		std::string str;
		for (int i = 0; i < length; i++)
		{
			str.push_back(m_BufferData[m_ReadIndex++]);
		}
		return str;
	}
};
