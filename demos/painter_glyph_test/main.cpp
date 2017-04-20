#include <iostream>
#include <sstream>
#include <fstream>
#include <fastuidraw/text/glyph_cache.hpp>
#include <fastuidraw/text/freetype_font.hpp>
#include <fastuidraw/text/glyph_selector.hpp>

#include "sdl_painter_demo.hpp"
#include "PanZoomTracker.hpp"
#include "text_helper.hpp"
#include "cycle_value.hpp"

using namespace fastuidraw;


class GlyphDraws:fastuidraw::noncopyable
{
public:
  ~GlyphDraws();

  void
  set_data(float pixel_size,
           const_c_array<vec2> in_glyph_positions,
           const_c_array<Glyph> in_glyphs,
           size_t glyphs_per_painter_draw);

  unsigned int
  size(void) const;
    
  const PainterAttributeData&
  data(unsigned int I) const;
    
private:
  std::vector<PainterAttributeData*> m_data;
};

class painter_glyph_test:public sdl_painter_demo
{
public:
  painter_glyph_test(void);
  ~painter_glyph_test();

protected:

  virtual
  void
  derived_init(int w, int h);

  virtual
  void
  draw_frame(void);

  virtual
  void
  handle_event(const SDL_Event&);

private:

  enum
    {
      draw_glyph_coverage,
      draw_glyph_curvepair,
      draw_glyph_distance,

      number_draw_modes
    };

  class PerLine
  {
  public:
    std::map<float, unsigned int> m_glyph_map;
    LineData m_L;
  };

  enum return_code
  create_and_add_font(void);

  void
  ready_glyph_attribute_data(void);

  void
  compute_glyphs_and_positions(GlyphRender renderer,
                               float pixel_size_formatting,
                               std::vector<Glyph> &out_glyphs);

  void
  change_glyph_renderer(GlyphRender renderer,
                        const_c_array<Glyph> in_glyphs,
                        std::vector<Glyph> &out_glyphs);

  float
  update_cts_params(void);

  void
  generate_glyph_at_data(std::vector<LineData> &in_data);
  
  unsigned int
  glyph_source_index(vec2 p);
  
  command_line_argument_value<std::string> m_font_path;
  command_line_argument_value<std::string> m_font_style, m_font_family;
  command_line_argument_value<bool> m_font_bold, m_font_italic;
  command_line_argument_value<int> m_coverage_pixel_size;
  command_line_argument_value<int> m_distance_pixel_size;
  command_line_argument_value<float> m_max_distance;
  command_line_argument_value<int> m_curve_pair_pixel_size;
  command_line_argument_value<std::string> m_text;
  command_line_argument_value<bool> m_use_file;
  command_line_argument_value<bool> m_draw_glyph_set;
  command_line_argument_value<float> m_render_pixel_size;
  command_line_argument_value<float> m_change_stroke_width_rate;
  command_line_argument_value<int> m_glyphs_per_painter_draw;

  reference_counted_ptr<const FontFreeType> m_font;

  vecN<GlyphDraws, number_draw_modes> m_draws;
  vecN<std::string, number_draw_modes + 1> m_draw_labels;
  vecN<std::vector<Glyph>, number_draw_modes> m_glyphs;
  std::vector<vec2> m_glyph_positions;
  std::vector<uint32_t> m_character_codes;
  std::map<float, PerLine> m_glyph_at;

  bool m_use_anisotropic_anti_alias;
  bool m_stroke_glyphs;
  bool m_draw_stats;
  float m_stroke_width;
  unsigned int m_current_drawer;
  PanZoomTrackerSDLEvent m_zoomer;
  simple_time m_draw_timer;
};


////////////////////////////////////
// GlyphDraws methods
GlyphDraws::
~GlyphDraws()
{
  for(unsigned int i = 0, endi = m_data.size(); i < endi; ++i)
    {
      FASTUIDRAWdelete(m_data[i]);
    }
}

void
GlyphDraws::
set_data(float pixel_size,
         const_c_array<vec2> in_glyph_positions,
         const_c_array<Glyph> in_glyphs,
         size_t glyphs_per_painter_draw)
{
  while(!in_glyphs.empty())
    {
      const_c_array<Glyph> glyphs;
      const_c_array<vec2> glyph_positions;
      unsigned int cnt;
      PainterAttributeData *data;
          
      cnt = t_min(in_glyphs.size(), glyphs_per_painter_draw);
      glyphs = in_glyphs.sub_array(0, cnt);
      glyph_positions = in_glyph_positions.sub_array(0, cnt);

      in_glyphs = in_glyphs.sub_array(cnt);
      in_glyph_positions = in_glyph_positions.sub_array(cnt);

      data = FASTUIDRAWnew PainterAttributeData();
      data->set_data(PainterAttributeDataFillerGlyphs(glyph_positions, glyphs, pixel_size));
      m_data.push_back(data);
    }
}

unsigned int
GlyphDraws::
size(void) const
{
  return m_data.size();
}
    
const PainterAttributeData&
GlyphDraws::
data(unsigned int I) const
{
  FASTUIDRAWassert(I < data.size());
  return *m_data[I];
}


/////////////////////////////////////
// painter_glyph_test methods
painter_glyph_test::
painter_glyph_test(void):
  m_font_path("/usr/share/fonts/truetype", "font_path", "Specifies path in which to search for fonts", *this),
  m_font_style("Book", "font_style", "Specifies the font style", *this),
  m_font_family("DejaVu Sans", "font_family", "Specifies the font family name", *this),
  m_font_bold(false, "font_bold", "if true select a bold font", *this),
  m_font_italic(false, "font_italic", "if true select an italic font", *this),
  m_coverage_pixel_size(24, "coverage_pixel_size", "Pixel size at which to create coverage glyphs", *this),
  m_distance_pixel_size(48, "distance_pixel_size", "Pixel size at which to create distance field glyphs", *this),
  m_max_distance(96.0f, "max_distance",
                 "value to use for max distance in 64'ths of a pixel "
                 "when generating distance field glyphs", *this),
  m_curve_pair_pixel_size(48, "curvepair_pixel_size", "Pixel size at which to create distance curve pair glyphs", *this),
  m_text("Hello World!", "text", "text to draw to the screen", *this),
  m_use_file(false, "use_file", "if true the value for text gives a filename to display", *this),
  m_draw_glyph_set(false, "draw_glyph_set", "if true, display all glyphs of font instead of text", *this),
  m_render_pixel_size(24.0f, "render_pixel_size", "pixel size at which to display glyphs", *this),
  m_change_stroke_width_rate(10.0f, "change_stroke_width_rate",
                             "rate of change in pixels/sec for changing stroke width "
                             "when changing stroke when key is down",
                             *this),
  m_glyphs_per_painter_draw(10000, "glyphs_per_painter_draw",
                            "Number of glyphs to draw per Painter::draw_text call",
                            *this),
  m_use_anisotropic_anti_alias(false),
  m_stroke_glyphs(false),
  m_draw_stats(false),
  m_stroke_width(1.0f),
  m_current_drawer(draw_glyph_curvepair)
{
  std::cout << "Controls:\n"
            << "\td:cycle drawing mode: draw coverage glyph, draw distance glyphs "
            << "[hold shift, control or mode to reverse cycle]\n"
            << "\ta:Toggle using anistropic anti-alias glyph rendering\n"
            << "\td:Cycle though text renderer\n"
            << "\tz:reset zoom factor to 1.0\n"
            << "\ts: toggle stroking glyph path\n"
            << "\tl: draw Painter stats\n"
            << "\t[: decrease stroke width(hold left-shift for slower rate and right shift for faster)\n"
            << "\t]: increase stroke width(hold left-shift for slower rate and right shift for faster)\n"
            << "\tMouse Drag (left button): pan\n"
            << "\tHold Mouse (left button), then drag up/down: zoom out/in\n";
}

painter_glyph_test::
~painter_glyph_test()
{
}

void
painter_glyph_test::
generate_glyph_at_data(std::vector<LineData> &in_data)
{
  for(unsigned int i = 0, endi = in_data.size(); i < endi; ++i)
    {
      const LineData &L(in_data[i]);
      PerLine &entry(m_glyph_at[L.m_vertical_spread.m_end]);

      entry.m_L = L;
      for(unsigned int g = L.m_range.m_begin; g < L.m_range.m_end; ++g)
        {
          float x_pos(m_glyph_positions[g].x());
          entry.m_glyph_map[x_pos] = g;
        }
    }
}

unsigned int
painter_glyph_test::
glyph_source_index(vec2 p)
{
  std::map<float, PerLine>::const_iterator iter;


  iter = m_glyph_at.lower_bound(p.y());
  if(iter != m_glyph_at.end()
     && iter->second.m_L.m_vertical_spread.m_begin <= p.y()
     && iter->second.m_L.m_horizontal_spread.m_begin <= p.x()
     && iter->second.m_L.m_horizontal_spread.m_end >= p.x()
     && !iter->second.m_glyph_map.empty())
    {
      const PerLine &L(iter->second);
      std::map<float, unsigned int>::const_iterator g_iter;

      g_iter = L.m_glyph_map.lower_bound(p.x());
      if(g_iter != L.m_glyph_map.end() && g_iter != L.m_glyph_map.begin())
        {
          --g_iter;
          return g_iter->second;
        }
      else
        {
          return L.m_glyph_map.rbegin()->second;
        }
    }
  return ~0u;
}

enum return_code
painter_glyph_test::
create_and_add_font(void)
{
  FontProperties props;
  props.style(m_font_style.m_value.c_str());
  props.family(m_font_family.m_value.c_str());
  props.bold(m_font_bold.m_value);
  props.italic(m_font_italic.m_value);

  add_fonts_from_path(m_font_path.m_value, m_ft_lib, m_glyph_selector,
                      FontFreeType::RenderParams()
                      .distance_field_max_distance(m_max_distance.m_value)
                      .distance_field_pixel_size(m_distance_pixel_size.m_value)
                      .curve_pair_pixel_size(m_curve_pair_pixel_size.m_value));

  reference_counted_ptr<const FontBase> font;

  font = m_glyph_selector->fetch_font(props);
  std::cout << "Chose font:" << font->properties() << "\n";

  m_font = font.dynamic_cast_ptr<const FontFreeType>();

  return routine_success;
}


void
painter_glyph_test::
derived_init(int w, int h)
{
  FASTUIDRAWunused(w);
  FASTUIDRAWunused(h);

  if(create_and_add_font() == routine_fail)
    {
      end_demo(-1);
      return;
    }

  //put into unit of per us
  m_change_stroke_width_rate.m_value /= (1000.0f * 1000.0f);

  ready_glyph_attribute_data();
  m_draw_timer.restart();
}


void
painter_glyph_test::
change_glyph_renderer(GlyphRender renderer,
                      const_c_array<Glyph> in_glyphs,
                      std::vector<Glyph> &out_glyphs)
{
  out_glyphs.resize(in_glyphs.size());
  for(unsigned int i = 0; i < in_glyphs.size(); ++i)
    {
      if(in_glyphs[i].valid())
        {
          out_glyphs[i] = m_glyph_selector->fetch_glyph_no_merging(renderer, in_glyphs[i].layout().m_font, m_character_codes[i]);
        }
    }
}

void
painter_glyph_test::
compute_glyphs_and_positions(fastuidraw::GlyphRender renderer,
                             float pixel_size_formatting,
                             std::vector<Glyph> &out_glyphs)
{
  if(m_draw_glyph_set.m_value)
    {
      float tallest(0.0f), negative_tallest(0.0f), offset;
      unsigned int i, endi, glyph_at_start, navigator_chars;
      float scale_factor, div_scale_factor;
      std::list< std::pair<float, std::string> > navigator;
      std::list< std::pair<float, std::string> >::iterator nav_iter;
      float line_length(800);
      FT_ULong character_code;
      FT_UInt  glyph_index;

      div_scale_factor = static_cast<float>(m_font->face()->units_per_EM);
      scale_factor = pixel_size_formatting / div_scale_factor;
      
      for(character_code = FT_Get_First_Char(m_font->face(), &glyph_index);
          glyph_index != 0;
          character_code = FT_Get_Next_Char(m_font->face(), character_code, &glyph_index))
        {
          Glyph g;
          g = m_glyph_selector->fetch_glyph_no_merging(renderer, m_font, character_code);

          FASTUIDRAWassert(g.valid());
          FASTUIDRAWassert(g.layout().m_glyph_code == uint32_t(glyph_index));
          tallest = std::max(tallest, g.layout().m_horizontal_layout_offset.y() + g.layout().m_size.y());
          negative_tallest = std::min(negative_tallest, g.layout().m_horizontal_layout_offset.y());
          out_glyphs.push_back(g);
          m_character_codes.push_back(character_code);
        }

      tallest *= scale_factor;
      negative_tallest *= scale_factor;
      offset = tallest - negative_tallest;

      m_glyph_positions.resize(out_glyphs.size());

      std::vector<LineData> lines;
      vec2 pen(0.0f, 0.0f);
      
      for(navigator_chars = 0, i = 0, endi = out_glyphs.size(), glyph_at_start = 0; i < endi; ++i)
        {
          Glyph g;
          float advance, nxt;

          g = out_glyphs[i];
          advance = scale_factor * std::max(g.layout().m_advance.x(), g.layout().m_size.x());

          m_glyph_positions[i].x() = pen.x();
          m_glyph_positions[i].y() = pen.y() + offset;
          pen.x() += advance;

          nxt = (i + 1 < endi) ?
            pen.x() + scale_factor * std::max(out_glyphs[i + 1].layout().m_advance.x(),
                                              out_glyphs[i + 1].layout().m_size.x()) :
            pen.x();
          
          if(nxt >= line_length || i + 1 == endi)
            {              
              std::ostringstream desc;
              desc << "[" << std::setw(5) << out_glyphs[glyph_at_start].layout().m_glyph_code
                   << " - " << std::setw(5) << out_glyphs[i].layout().m_glyph_code << "]";
              navigator.push_back(std::make_pair(pen.y(), desc.str()));
              navigator_chars += navigator.back().second.length();

              LineData L;
              L.m_range.m_begin = glyph_at_start;
              L.m_range.m_end = i + 1;
              L.m_vertical_spread.m_begin = pen.y() + offset - tallest;
              L.m_vertical_spread.m_end = pen.y() + offset - negative_tallest;
              L.m_horizontal_spread.m_begin = 0.0f;
              L.m_horizontal_spread.m_end = pen.x();
              lines.push_back(L);
              glyph_at_start = i + 1;

              pen.x() = 0.0f;
              pen.y() += offset + 1.0f;
            }
        }

      generate_glyph_at_data(lines);

      m_character_codes.reserve(out_glyphs.size() + navigator_chars);
      m_glyph_positions.reserve(out_glyphs.size() + navigator_chars);
      out_glyphs.reserve(out_glyphs.size() + navigator_chars);

      std::vector<fastuidraw::Glyph> temp_glyphs;
      std::vector<fastuidraw::vec2> temp_positions;
      std::vector<uint32_t> temp_character_codes;
      for(nav_iter = navigator.begin(); nav_iter != navigator.end(); ++nav_iter)
        {
          std::istringstream stream(nav_iter->second);

          temp_glyphs.clear();
          temp_positions.clear();
          temp_character_codes.clear();
          create_formatted_text(stream, renderer, pixel_size_formatting,
                                m_font, m_glyph_selector, temp_glyphs,
                                temp_positions, temp_character_codes);

          FASTUIDRAWassert(temp_glyphs.size() == temp_positions.size());
          for(unsigned int c = 0; c < temp_glyphs.size(); ++c)
            {
              out_glyphs.push_back(temp_glyphs[c]);
              m_character_codes.push_back(temp_character_codes[c]);
              m_glyph_positions.push_back(vec2(line_length + temp_positions[c].x(), nav_iter->first) );
            }
        }
    }
  else if(m_use_file.m_value)
    {
      std::ifstream istr(m_text.m_value.c_str(), std::ios::binary);
      if(istr)
        {
          std::vector<LineData> lines;
          create_formatted_text(istr, renderer, pixel_size_formatting,
                                m_font, m_glyph_selector,
                                out_glyphs, m_glyph_positions,
                                m_character_codes, &lines);
          generate_glyph_at_data(lines);
        }
    }
  else
    {
      std::vector<LineData> lines;
      std::istringstream istr(m_text.m_value);
      create_formatted_text(istr, renderer, pixel_size_formatting,
                            m_font, m_glyph_selector,
                            out_glyphs, m_glyph_positions,
                            m_character_codes, &lines);
      generate_glyph_at_data(lines);
    }
}



void
painter_glyph_test::
ready_glyph_attribute_data(void)
{
  {
    GlyphRender renderer(m_coverage_pixel_size.m_value);
    compute_glyphs_and_positions(renderer, m_render_pixel_size.m_value,
                                 m_glyphs[draw_glyph_coverage]);
    m_draws[draw_glyph_coverage].set_data(m_render_pixel_size.m_value,
                                          cast_c_array(m_glyph_positions),
                                          cast_c_array(m_glyphs[draw_glyph_coverage]),
                                          m_glyphs_per_painter_draw.m_value);
    m_draw_labels[draw_glyph_coverage] = "draw_glyph_coverage";
  }

  {
    GlyphRender renderer(distance_field_glyph);
    change_glyph_renderer(renderer,
                          cast_c_array(m_glyphs[draw_glyph_coverage]),
                          m_glyphs[draw_glyph_distance]);
    m_draws[draw_glyph_distance].set_data(m_render_pixel_size.m_value,
                                          cast_c_array(m_glyph_positions),
                                          cast_c_array(m_glyphs[draw_glyph_distance]),
                                          m_glyphs_per_painter_draw.m_value);
    m_draw_labels[draw_glyph_distance] = "draw_glyph_distance";
  }

  {
    GlyphRender renderer(curve_pair_glyph);
    change_glyph_renderer(renderer,
                          cast_c_array(m_glyphs[draw_glyph_coverage]),
                          m_glyphs[draw_glyph_curvepair]);
    m_draws[draw_glyph_curvepair].set_data(m_render_pixel_size.m_value,
                                           cast_c_array(m_glyph_positions),
                                           cast_c_array(m_glyphs[draw_glyph_curvepair]),
                                           m_glyphs_per_painter_draw.m_value);
    m_draw_labels[draw_glyph_curvepair] = "draw_glyph_curvepair";
  }

  m_draw_labels[number_draw_modes] = "draw_glyph_as_filled_paths";
}

void
painter_glyph_test::
draw_frame(void)
{
  float us;

  us = update_cts_params();

  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  m_painter->begin();
  ivec2 wh(dimensions());
  float3x3 proj(float_orthogonal_projection_params(0, wh.x(), wh.y(), 0)), m;
  m = proj * m_zoomer.transformation().matrix3();
  m_painter->transformation(m);

  PainterBrush brush;
  brush.pen(1.0, 1.0, 1.0, 1.0);

  if(m_current_drawer != number_draw_modes)
    {
      for(unsigned int S = 0, endS = m_draws[m_current_drawer].size(); S < endS; ++S)
        {
          m_painter->draw_glyphs(PainterData(&brush),
                                 m_draws[m_current_drawer].data(S),
                                 m_use_anisotropic_anti_alias);
        }
    }
  else
    {
      unsigned int src(draw_glyph_curvepair);
      PainterBrush fill_brush;
      fill_brush.pen(1.0, 1.0, 1.0, 1.0);

      // reuse brush parameters across all glyphs
      PainterPackedValue<PainterBrush> pbr;
      pbr = m_painter->packed_value_pool().create_packed_value(fill_brush);
      for(unsigned int i = 0; i < m_glyphs[src].size(); ++i)
        {
          if(m_glyphs[src][i].valid())
            {
              m_painter->save();
              m_painter->translate(m_glyph_positions[i]);

              //make the scale of the path match how we scaled the text.
              float sc;
              sc = m_render_pixel_size.m_value / m_glyphs[src][i].layout().m_units_per_EM;
              m_painter->scale(sc);
              m_painter->fill_path(PainterData(pbr),
                                   m_glyphs[src][i].path(),
                                   PainterEnums::nonzero_fill_rule,
                                   true);
              m_painter->restore();
            }
        }
    }

  if(m_stroke_glyphs)
    {
      unsigned int src;
      PainterBrush stroke_brush;
      stroke_brush.pen(0.0, 1.0, 1.0, 0.8);

      PainterStrokeParams st;
      st.miter_limit(5.0f);
      st.width(m_stroke_width);

      src = (m_current_drawer == number_draw_modes) ?
        static_cast<unsigned int>(draw_glyph_curvepair) :
        m_current_drawer;
      
      // reuse stroke and brush parameters across all glyphs
      PainterPackedValue<PainterBrush> pbr;
      pbr = m_painter->packed_value_pool().create_packed_value(stroke_brush);

      PainterPackedValue<PainterItemShaderData> pst;
      pst = m_painter->packed_value_pool().create_packed_value(st);

      for(unsigned int i = 0; i < m_glyphs[src].size(); ++i)
        {
          if(m_glyphs[src][i].valid())
            {
              m_painter->save();
              m_painter->translate(m_glyph_positions[i]);

              //make the scale of the path match how we scaled the text.
              float sc;
              sc = m_render_pixel_size.m_value / m_glyphs[src][i].layout().m_units_per_EM;
              m_painter->scale(sc);
              m_painter->stroke_path_pixel_width(PainterData(pst, pbr),
                                                 m_glyphs[src][i].path(),
                                                 true, PainterEnums::flat_caps, PainterEnums::miter_bevel_joins,
                                                 true);
              m_painter->restore();
            }
        }
    }

  if(m_draw_stats)
    {
      std::ostringstream ostr;
      
      ostr << "FPS = ";
      if(us > 0.0f)
        {
          ostr << 1000.0f * 1000.0f / us;
        }
      else
        {
          ostr << "NAN";
        }

      ostr << "\nms = " << us / 1000.0f
           << "\nAttribs: "
           << m_painter->query_stat(PainterPacker::num_attributes)
           << "\nIndices: "
           << m_painter->query_stat(PainterPacker::num_indices)
           << "\nGenericData: "
           << m_painter->query_stat(PainterPacker::num_generic_datas)
           << "\n";

      m_painter->transformation(proj);
      PainterBrush brush;
      brush.pen(0.0f, 1.0f, 1.0f, 1.0f);
      draw_text(ostr.str(), 32.0f, m_font, GlyphRender(curve_pair_glyph), PainterData(&brush));
    }
  else
    {
      vec2 p;
      unsigned int G;
      ivec2 mouse_position;
      std::ostringstream ostr;

      SDL_GetMouseState(&mouse_position.x(), &mouse_position.y());
      p = m_zoomer.transformation().apply_inverse_to_point(vec2(mouse_position));
      G = glyph_source_index(p);
      if(G != ~0u)
        {
          unsigned int src;
          Glyph glyph;
          GlyphLayoutData layout;
          float ratio;

          src = (m_current_drawer == number_draw_modes) ?
            static_cast<unsigned int>(draw_glyph_curvepair) :
            m_current_drawer;

          glyph = m_glyphs[src][G];
          layout = glyph.layout();
          ratio = m_render_pixel_size.m_value / layout.m_units_per_EM;
          
          ostr << "Glyph at " << p << " is:"
               << "\n\tcharacter_code: " << m_character_codes[G]
               << "\n\tglyph_code: " << layout.m_glyph_code
               << "\n\tunits_per_EM: " << layout.m_units_per_EM
               << "\n\tsize in EM: " << layout.m_size
               << "\n\tsize normalized: " << layout.m_size * ratio
               << "\n";
          /* draw a box around the glyph(!).
           */
          PainterBrush brush;
          vec2 wh;

          brush.pen(1.0f, 0.0f, 0.0f, 0.3f);
          p.x() = m_glyph_positions[G].x() + ratio * layout.m_horizontal_layout_offset.x();
          p.y() = m_glyph_positions[G].y() - ratio * layout.m_horizontal_layout_offset.y();
          wh.x() = ratio * layout.m_size.x();
          wh.y() = -ratio * layout.m_size.y();
          m_painter->draw_rect(PainterData(&brush), p, wh, false);
        }
      else
        {
          ostr << "No glyph at " << p << "\n";
        }

      m_painter->transformation(proj);
      PainterBrush brush;
      brush.pen(0.0f, 1.0f, 1.0f, 1.0f);
      draw_text(ostr.str(), 32.0f, m_font, GlyphRender(curve_pair_glyph), PainterData(&brush));
    }

  m_painter->end();
}

float
painter_glyph_test::
update_cts_params(void)
{
  float return_value;
  const Uint8 *keyboard_state = SDL_GetKeyboardState(nullptr);
  FASTUIDRAWassert(keyboard_state != nullptr);

  float speed;
  return_value = static_cast<float>(m_draw_timer.restart_us());
  speed = return_value * m_change_stroke_width_rate.m_value;

  if(keyboard_state[SDL_SCANCODE_LSHIFT])
    {
      speed *= 0.1f;
    }
  if(keyboard_state[SDL_SCANCODE_RSHIFT])
    {
      speed *= 10.0f;
    }

  if(keyboard_state[SDL_SCANCODE_RIGHTBRACKET])
    {
      m_stroke_width += speed;
    }

  if(keyboard_state[SDL_SCANCODE_LEFTBRACKET] && m_stroke_width > 0.0f)
    {
      m_stroke_width -= speed;
      m_stroke_width = fastuidraw::t_max(m_stroke_width, 0.0f);
    }

  if(keyboard_state[SDL_SCANCODE_RIGHTBRACKET] || keyboard_state[SDL_SCANCODE_LEFTBRACKET])
    {
      std::cout << "Stroke width set to: " << m_stroke_width << "\n";
    }
  return return_value;
}

void
painter_glyph_test::
handle_event(const SDL_Event &ev)
{
  m_zoomer.handle_event(ev);
  switch(ev.type)
    {
    case SDL_QUIT:
      end_demo(0);
      break;

    case SDL_WINDOWEVENT:
      if(ev.window.event == SDL_WINDOWEVENT_RESIZED)
        {
          on_resize(ev.window.data1, ev.window.data2);
        }
      break;

    case SDL_KEYUP:
      switch(ev.key.keysym.sym)
        {
        case SDLK_ESCAPE:
          end_demo(0);
          break;

        case SDLK_a:
          m_use_anisotropic_anti_alias = !m_use_anisotropic_anti_alias;
          if(m_use_anisotropic_anti_alias)
            {
              std::cout << "Using Anistropic anti-alias filtering\n";
            }
          else
            {
              std::cout << "Using Istropic anti-alias filtering\n";
            }
          break;

        case SDLK_d:
          cycle_value(m_current_drawer, ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT), number_draw_modes + 1);
          std::cout << "Drawing " << m_draw_labels[m_current_drawer] << " glyphs\n";
          break;

        case SDLK_z:
          {
            vec2 p, fixed_point(dimensions() / 2);
            ScaleTranslate<float> tr;
            tr = m_zoomer.transformation();
            p = fixed_point - (fixed_point - tr.translation()) / tr.scale();
            m_zoomer.transformation(ScaleTranslate<float>(p));
          }
          break;

        case SDLK_s:
          {
            m_stroke_glyphs = !m_stroke_glyphs;
            std::cout << "Set to ";
            if(!m_stroke_glyphs)
              {
                std::cout << "not ";
              }
            std::cout << " stroke glyph paths\n";
          }
          break;

        case SDLK_l:
          m_draw_stats = !m_draw_stats;
          break;
        }
      break;
    }
}

int
main(int argc, char **argv)
{
  painter_glyph_test G;
  return G.main(argc, argv);
}
