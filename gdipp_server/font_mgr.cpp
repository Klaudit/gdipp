#include "stdafx.h"
#include "font_mgr.h"
#include "gdipp_lib/lock.h"
#include "gdipp_server/global.h"

namespace gdipp
{

void *font_mgr::register_font(const LOGFONTW *attr_buf, DWORD buf_size, HDC hdc)
{
	const HFONT linked_hfont = CreateFontIndirectW(attr_buf);
	if (linked_hfont == NULL)
		return NULL;

	HDC font_holder;
	if (hdc == NULL)
	{
		font_holder = dc_pool_instance.claim();
		assert(font_holder != NULL);
	}
	else
	{
		font_holder = hdc;
		SelectObject(font_holder, linked_hfont);
	}

	std::vector<BYTE> metric_buf;
	OUTLINETEXTMETRICW *outline_metrics;
	outline_metrics = get_dc_metrics(font_holder, metric_buf);

	if (hdc == NULL)
		dc_pool_instance.free(font_holder);

	if (outline_metrics == NULL)
		return NULL;

	std::wstring font_face = metric_face_name(outline_metrics);
	std::map<std::wstring, font_entry>::const_iterator font_iter = _font_registry.find(font_face);
	if (font_iter == _font_registry.end())
	{
		lock l(lock::SERVER_FONT_MGR);
		font_iter = _font_registry.find(font_face);
		if (font_iter == _font_registry.end())
		{
			const font_entry new_font_data = {linked_hfont, metric_buf};
			_font_registry[font_face] = new_font_data;

			return &_font_registry[font_face];
		}
	}

	// font existed, use existing font
	DeleteObject(linked_hfont);

	return const_cast<font_entry *>(&font_iter->second);
}

DWORD font_mgr::get_font_data(void *font_id, DWORD table, DWORD offset, LPVOID data_buf, DWORD buf_size, HDC hdc) const
{
	const font_entry *curr_font = reinterpret_cast<const font_entry *>(font_id);
	HDC font_holder;
	
	if (hdc == NULL)
	{
		font_holder = dc_pool_instance.claim();
		assert(font_holder != NULL);
	}
	else
	{
		font_holder = hdc;
	}
	SelectObject(font_holder, curr_font->font_handle);

	const DWORD data_size = GetFontData(font_holder, table, offset, data_buf, buf_size);

	if (hdc == NULL)
		dc_pool_instance.free(font_holder);

	return data_size;
}

const std::vector<BYTE> *font_mgr::get_font_metrics(void *font_id) const
{
	const font_entry *curr_font = reinterpret_cast<const font_entry *>(font_id);

	return &curr_font->metric_buf;
}

DWORD font_mgr::get_glyph_indices(void *font_id, const wchar_t *str, int count, unsigned short *gi, HDC hdc) const
{
	const font_entry *curr_font = reinterpret_cast<const font_entry *>(font_id);
	HDC font_holder;
	
	if (hdc == NULL)
	{
		font_holder = dc_pool_instance.claim();
		assert(font_holder != NULL);
	}
	else
	{
		font_holder = hdc;
	}
	SelectObject(font_holder, curr_font->font_handle);

	const DWORD converted = GetGlyphIndices(font_holder, str, count, gi, GGI_MARK_NONEXISTING_GLYPHS);

	if (hdc == NULL)
		dc_pool_instance.free(font_holder);

	return converted;
}

HFONT font_mgr::select_font(void *font_id, HDC hdc) const
{
	const font_entry *curr_font = reinterpret_cast<const font_entry *>(font_id);

	return reinterpret_cast<HFONT>(SelectObject(hdc, curr_font->font_handle));
}

FT_Stream font_mgr::lookup_stream(void *font_id) const
{
	return NULL;
}

ULONG font_mgr::lookup_face_index(void *font_id) const
{
	return 0;
}

OUTLINETEXTMETRICW *font_mgr::get_dc_metrics(HDC hdc, std::vector<BYTE> &metric_buf)
{
	// get outline metrics of the DC, which also include the text metrics

	UINT metric_size = GetOutlineTextMetricsW(hdc, 0, NULL);
	if (metric_size == 0)
		return NULL;

	metric_buf.resize(metric_size);
	OUTLINETEXTMETRICW *outline_metrics = reinterpret_cast<OUTLINETEXTMETRICW *>(&metric_buf[0]);
	metric_size = GetOutlineTextMetricsW(hdc, metric_size, outline_metrics);
	assert(metric_size != 0);

	return outline_metrics;
}

unsigned long font_mgr::stream_io(FT_Stream stream, unsigned long offset, unsigned char *buffer, unsigned long count)
{
	// callback function, called when freetype requests font data

	return 0;
}

void font_mgr::stream_close(FT_Stream stream)
{
	// GetFontData() needs no close
}

}