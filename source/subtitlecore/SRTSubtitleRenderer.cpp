#include "stdafx.h"
#include "SRTSubtitleRenderer.h"
#include "SRTSubtitleEntry.h"

#include "TimeConversion.h"

using namespace SubtitleCore;

SRTSubtitleRenderer::SRTSubtitleRenderer(SubtitleCoreConfigurationData& config, VideoInfo& vidInfo, IDWriteFactory* dwFactory)
	: m_SubCoreConfig(config)
	, m_VideoInfo(vidInfo)
	, m_DWriteFactory(dwFactory)
{
	m_SubtitleType = SBT_SRT;
}

bool SRTSubtitleRenderer::ParseScript(const std::vector<std::wstring>& script)
{
	for (size_t i = 0; i < script.size(); i++)
	{
		// Ignore lines which are just the subtitle entry number
		if (script[i].find_first_not_of(L"0123456789") != std::wstring::npos)
		{
			// This should be a timestamp
			if (CheckLineIsTimestamp(script[i]))
			{
				REFERENCE_TIME rtStart, rtEnd;

				ComputeTimestamp(script[i], rtStart, rtEnd);

				// Merge lines until we reach an empty line which delineates the start of a new
				// subtitle block
				std::wstring mergedLine;
				size_t index = i+1;
				while (script[index].empty() == false)
				{
					mergedLine += script[index] + L"\n";
					++index;
				}
				// Move overall script parsing position to start of the next subtitle block
				i = index+1;

				// Parse merged line
				ParseLine(mergedLine, rtStart, rtEnd);
			}
			else
			{
				// Something bad happened
				return false;
			}
		}
	}

	return true;
}

bool SRTSubtitleRenderer::ParseLine(const std::wstring& line)
{
	UNREFERENCED_PARAMETER(line);
	return false;
}

bool SRTSubtitleRenderer::ParseLine(const std::wstring& line, REFERENCE_TIME rtStart, REFERENCE_TIME rtEnd)
{
	SRTSubtitleEntry* entry = nullptr;

	auto& subtitle_list = m_SubtitleMap[rtStart];

	if (subtitle_list.size() == 0)
	{
		// Subtitle list with this start time was empty so add a new entry
		subtitle_list.push_back(SRTSubtitleEntry());

		entry = &subtitle_list[0];
		entry->EndTime = rtEnd;
	}
	else
	{
		subtitle_list.push_back(SRTSubtitleEntry());

		entry = &subtitle_list[subtitle_list.size() - 1];
		entry->EndTime = rtEnd;
	}

	std::wstring finalLine;

	// Check for supported HTML tags
	size_t startPos, openBracketPos, closeBracketPos;
	size_t finalStartPos, finalEndPos;
	std::vector<std::wstring> tagStack;

	startPos = 0;

	while ((openBracketPos = line.find(L"<", startPos)) != std::wstring::npos)
	{
		finalStartPos = finalLine.size();
		finalLine += line.substr(startPos, openBracketPos - startPos);
		finalEndPos = finalLine.size();

		if (finalEndPos != finalStartPos)
		{
			SRTSubtitleEntry::TextRangeFormat trf;

			trf.Range.length = finalEndPos - finalStartPos;
			trf.Range.startPosition = finalStartPos;
			trf.Style = m_SubCoreConfig.m_FontStyle;
			trf.Weight = m_SubCoreConfig.m_FontWeight;
			trf.Strikethrough = FALSE;
			trf.Underline = FALSE;

			for (auto it = tagStack.begin(); it != tagStack.end(); ++it)
			{
				if ((*it).compare(L"b") == 0)
				{
					trf.Weight = DWRITE_FONT_WEIGHT_BOLD;
				}
				else if ((*it).compare(L"i") == 0)
				{
					trf.Style = DWRITE_FONT_STYLE_ITALIC;
				}
				else if ((*it).compare(L"u") == 0)
				{
					trf.Underline = TRUE;
				}
				else if ((*it).compare(L"s") == 0)
				{
					trf.Strikethrough = TRUE;
				}
			}

			entry->SubTextFormat.push_back(trf);
		}

		// Find close bracket
		closeBracketPos = line.find(L">", openBracketPos);

		std::wstring tag = line.substr(openBracketPos+1, closeBracketPos - openBracketPos - 1);

		// Is this a closing tag?
		if (tag.find(L"/") != std::wstring::npos)
		{
			// Malformed scripts are just going to break
			tagStack.pop_back();
		}
		else
		{
			tagStack.push_back(tag);
		}

		startPos = closeBracketPos+1;
	}

	if (startPos != line.size() - 1)
	{
		// Copy remainder line
		finalLine += line.substr(startPos);
	}

	entry->Text = finalLine;

	return true;

}

bool SRTSubtitleRenderer::ParseFormatHeader(const std::wstring& header)
{
	UNREFERENCED_PARAMETER(header);
	return true;
}

bool SRTSubtitleRenderer::ParseFormatHeader(const unsigned char* header)
{
	UNREFERENCED_PARAMETER(header);
	return true;
}

bool SRTSubtitleRenderer::ParseData(const unsigned char* data)
{
	UNREFERENCED_PARAMETER(data);
	return false;
}

bool SRTSubtitleRenderer::ParseData(const unsigned char* data, ptrdiff_t offset)
{
	UNREFERENCED_PARAMETER(data);
	UNREFERENCED_PARAMETER(offset);
	return false;
}

bool SRTSubtitleRenderer::ParseData(const unsigned char* data, ptrdiff_t startOffset, ptrdiff_t endOffset)
{
	UNREFERENCED_PARAMETER(data);
	UNREFERENCED_PARAMETER(startOffset);
	UNREFERENCED_PARAMETER(endOffset);
	return false;
}

void SRTSubtitleRenderer::Invalidate()
{
	m_SubtitleMap.clear();
}

size_t SRTSubtitleRenderer::GetSubtitlePictureCount(REFERENCE_TIME rtNow)
{
	UNREFERENCED_PARAMETER(rtNow);
	return 0U;
}

void SRTSubtitleRenderer::GetSubtitlePicture(REFERENCE_TIME rtNow, SubtitlePicture** ppOutSubPics)
{
	UNREFERENCED_PARAMETER(rtNow);
	UNREFERENCED_PARAMETER(ppOutSubPics);
}

bool SRTSubtitleRenderer::CheckLineIsTimestamp(const std::wstring& line)
{
	return (line.find(L"-->") != std::wstring::npos &&
		line.find(L":") != std::wstring::npos && 
		line.find(L",") != std::wstring::npos);
}

void SRTSubtitleRenderer::ComputeTimestamp(const std::wstring& line, REFERENCE_TIME& rtStart, REFERENCE_TIME& rtEnd)
{
	size_t startPos, endPos;
	size_t hours, minutes, seconds, milliseconds;

	startPos = 0;

	// Get hours
	endPos = line.find(L":");
	hours = boost::lexical_cast<size_t>(line.substr(startPos, endPos - startPos));

	// Get minutes
	startPos = endPos + 1;
	endPos = line.find(L":", startPos);
	minutes = boost::lexical_cast<size_t>(line.substr(startPos, endPos - startPos));

	// Get seconds
	startPos = endPos + 1;
	endPos = line.find(L",", startPos);
	seconds = boost::lexical_cast<size_t>(line.substr(startPos, endPos - startPos));

	// Get milliseconds
	startPos = endPos + 1;
	endPos = line.find(L" ", startPos);
	milliseconds = boost::lexical_cast<size_t>(line.substr(startPos, endPos - startPos));

	// Compute starting time
	rtStart = SubtitleCoreUtilities::ConvertTimeToReferenceTime(hours, minutes, seconds, milliseconds);

	// Move to ending timestamp
	startPos = line.find(L" ", endPos+1) + 1;

	// Get hours
	endPos = line.find(L":", startPos);
	hours = boost::lexical_cast<size_t>(line.substr(startPos, endPos - startPos));

	// Get minutes
	startPos = endPos + 1;
	endPos = line.find(L":", startPos);
	minutes = boost::lexical_cast<size_t>(line.substr(startPos, endPos - startPos));

	// Get seconds
	startPos = endPos + 1;
	endPos = line.find(L",", startPos);
	seconds = boost::lexical_cast<size_t>(line.substr(startPos, endPos - startPos));

	// Get milliseconds
	startPos = endPos + 1;
	milliseconds = boost::lexical_cast<size_t>(line.substr(startPos));

	rtEnd = SubtitleCoreUtilities::ConvertTimeToReferenceTime(hours, minutes, seconds, milliseconds);
}
