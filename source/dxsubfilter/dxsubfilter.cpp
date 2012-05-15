// dxsubfilter.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "dxsubfilter.h"

using namespace DXSubFilter;

// This is the constructor of a class that has been exported.
// see dxsubfilter.h for the class definition
CDXSubFilter::CDXSubFilter(LPUNKNOWN pUnk) 
	: CTransformFilter(DXSUBFILTER_NAME, pUnk, CLSID_DXSubFilter)
	, m_pInputSubtitlePin(nullptr)
{
	// Just in case the CTransformFilter constructor doesn't default these to null
	m_pInput = nullptr;
	m_pOutput = nullptr;
}

CDXSubFilter::~CDXSubFilter()
{
	// Do I need to manually delete m_pInput and m_pOutput?
	delete m_pInputSubtitlePin;
}

CUnknown* CDXSubFilter::CreateInstance(LPUNKNOWN pUnk, HRESULT* phr)
{
	return new CDXSubFilter(pUnk);
}

int CDXSubFilter::GetPinCount()
{
	return m_iPinCount;
}

CBasePin* CDXSubFilter::GetPin(int n)
{
	HRESULT hr = S_OK;
	switch (n)
	{
	case 0:
		if (!m_pInput)
		{
			m_pInput = new CTransformInputPin(L"Video", this, &hr, L"Video");

			if (FAILED(hr))
			{
				return nullptr;
			}
		}
		return m_pInput;
	case 1:
		if (!m_pOutput)
		{
			m_pOutput = new CTransformOutputPin(L"Output", this, &hr, L"Output");

			if (FAILED(hr))
			{
				return nullptr;
			}
		}
		return m_pOutput;
	case 2:
		if (!m_pInputSubtitlePin)
		{
			m_pInputSubtitlePin = new CTransformInputPin(L"Subtitle", this, &hr, L"Subtitle");

			if (FAILED(hr))
			{
				return nullptr;
			}
		}
		return m_pInputSubtitlePin;
	default:
		return nullptr;
	}
}

bool CDXSubFilter::CheckVideoSubtypeIs8Bit(const CMediaType* pMediaType)
{
	bool result = false;

	GUID subtype = pMediaType->subtype;
	for (size_t i = 0; i < DXSUBFILTER_SUPPORTED_VIDEO_SUBTYPES_8BIT_COUNT; i++)
	{
		if (subtype == DXSUBFILTER_SUPPORTED_VIDEO_SUBTYPES_8BIT[i])
		{
			result = true;
			break;
		}
	}

	return result;
}

bool CDXSubFilter::CheckVideoSubtypeIs16Bit(const CMediaType* pMediaType)
{
	bool result = false;

	GUID subtype = pMediaType->subtype;
	for (size_t i = 0; i < DXSUBFILTER_SUPPORTED_VIDEO_SUBTYPES_16BIT_COUNT; i++)
	{
		if (subtype == DXSUBFILTER_SUPPORTED_VIDEO_SUBTYPES_16BIT[i])
		{
			result = true;
			break;
		}
	}

	return result;
}

HRESULT CDXSubFilter::CheckInputType(const CMediaType* mtIn)
{
	// We only accept video and text
	if (mtIn->majortype == MEDIATYPE_Video)
	{
		// Check to see if the video subtype is one of the supported subtypes
		GUID subtype = mtIn->subtype;
		for (size_t i = 0; i < DXSUBFILTER_SUPPORTED_VIDEO_SUBTYPES_8BIT_COUNT; i++)
		{
			if (subtype == DXSUBFILTER_SUPPORTED_VIDEO_SUBTYPES_8BIT[i])
			{
				return S_OK;
			}
		}

		for (size_t i = 0; i < DXSUBFILTER_SUPPORTED_VIDEO_SUBTYPES_16BIT_COUNT; i++)
		{
			if (subtype == DXSUBFILTER_SUPPORTED_VIDEO_SUBTYPES_16BIT[i])
			{
				return S_OK;
			}
		}

		// Not a supported subtype so fail
		return VFW_E_TYPE_NOT_ACCEPTED;
	}
	else if (mtIn->majortype == MEDIATYPE_Text || mtIn->majortype == MEDIATYPE_Subtitle)
	{
		return S_OK;
	}
	else
	{
		return VFW_E_TYPE_NOT_ACCEPTED;
	}
}

HRESULT CDXSubFilter::CheckTransform(const CMediaType* mtIn, const CMediaType* mtOut)
{
	// Don't know if this check is necessary
	if (mtIn->majortype == MEDIATYPE_Video)
	{
		// If the input matches the output, we automatically accept
		if (*mtIn == *mtOut)
		{
			return S_OK;
		}
		else
		{
			// We check to see if we're going from 10/16-bit input to 8-bit output
			if (CheckVideoSubtypeIs16Bit(mtIn) && CheckVideoSubtypeIs8Bit(mtOut))
			{
				// Force upstream to reconnect with the same proposed format as the output
				if (SUCCEEDED(m_pInput->QueryAccept(mtOut)))
				{
					if (SUCCEEDED(ReconnectPin(m_pInput, mtOut)))
					{
						m_pInput->SetMediaType(mtOut);
						
						return S_OK;
					}
					else
					{
						return VFW_E_TYPE_NOT_ACCEPTED;
					}
				}
				else
				{
					// If upstream doesn't accept the output format, much sadness occurs :(
					return VFW_E_TYPE_NOT_ACCEPTED;
				}
			}
			else
			{
				// Don't know what's going on at this point, but it doesn't matter because we don't
				// want it
				return VFW_E_TYPE_NOT_ACCEPTED;
			}
		}
	}
	return VFW_E_TYPE_NOT_ACCEPTED;
}

HRESULT CDXSubFilter::DecideBufferSize(IMemAllocator * pAllocator, ALLOCATOR_PROPERTIES *pprop)
{
	return S_OK;
}

HRESULT CDXSubFilter::GetMediaType(int iPosition, CMediaType *pMediaType)
{
	if (iPosition < 0)
	{
		return E_INVALIDARG;
	}
	else if (m_pInput->IsConnected() == TRUE)
	{
		// In the special case of 10/16-bit input, we will offer 8-bit formats as output so that
		// we can force the video decoder to output in 8-bit if the video renderer can't accept 
		// 10/16-bit input. Otherwise, we only offer the input media type as an output type.
		if (CheckVideoSubtypeIs16Bit(&m_InputVideoType))
		{
			// Try input format first
			if (iPosition == 0)
			{
				*pMediaType = m_InputVideoType;
			}
			else if (iPosition > DXSUBFILTER_SUPPORTED_VIDEO_SUBTYPES_8BIT_COUNT)
			{
				return VFW_S_NO_MORE_ITEMS;
			}
			else
			{
				// Subtract 1 from iPosition because iPosition == 0 is used for input format.
				pMediaType->SetSubtype(&DXSUBFILTER_SUPPORTED_VIDEO_SUBTYPES_8BIT[iPosition-1]);
			}
		}
		else
		{
			*pMediaType = m_InputVideoType;
		}
		return S_OK;
	}
	else
	{
		return VFW_S_NO_MORE_ITEMS;
	}
}

HRESULT CDXSubFilter::SetMediaType(PIN_DIRECTION direction,const CMediaType *pmt)
{
	// Call base class SetMediaType first and see if everything's ok
	HRESULT hr = CTransformFilter::SetMediaType(direction, pmt);
	if (FAILED(hr))
	{
		return hr;
	}
	else
	{
		// Save the input media type from video decoder
		if (direction == PINDIR_INPUT && pmt->majortype == MEDIATYPE_Video)
		{
			m_InputVideoType = *pmt;
		}
		return S_OK;
	}
}

HRESULT CDXSubFilter::Transform(IMediaSample * pIn, IMediaSample *pOut)
{
	return S_OK;
}

