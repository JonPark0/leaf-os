#include "text_wrap.h"

namespace ui::text {

std::vector<Line> wrap(const std::vector<std::string>& paragraphs, int width,
                       const Measure& measure)
{
    std::vector<Line> out;
    for (const std::string& p : paragraphs) {
        if (p.empty()) {
            out.push_back({"", true});
            continue;
        }
        size_t start = 0;
        while (start < p.size()) {
            size_t end = start, after_space = std::string::npos;
            while (end < p.size()) {
                size_t next = end + static_cast<size_t>(u8len(static_cast<unsigned char>(p[end])));
                if (next > p.size()) next = p.size();
                if (measure(p.c_str() + start, static_cast<int>(next - start)) > width) break;
                if (p[end] == ' ') after_space = next;
                end = next;
            }
            if (end < p.size() && after_space != std::string::npos && after_space > start)
                end = after_space;                       // break after the last space
            if (end == start) {                          // not even one scalar fits
                end = start + static_cast<size_t>(u8len(static_cast<unsigned char>(p[start])));
                if (end > p.size()) end = p.size();
            }
            size_t text_end = end;
            while (text_end > start && p[text_end - 1] == ' ') --text_end;   // no trailing blanks
            out.push_back({p.substr(start, text_end - start), false});
            start = end;
            while (start < p.size() && p[start] == ' ') ++start;            // nor leading ones
        }
    }
    return out;
}

std::vector<size_t> paginate(const std::vector<Line>& lines, int line_h, int gap_h, int page_h)
{
    std::vector<size_t> starts;
    size_t i = 0;
    while (i < lines.size() && lines[i].gap) ++i;
    starts.push_back(i);
    int used = 0;
    for (; i < lines.size(); ++i) {
        const int h = lines[i].gap ? gap_h : line_h;
        if (used > 0 && used + h > page_h) {
            if (lines[i].gap) {                          // a gap would open the page: skip it
                while (i < lines.size() && lines[i].gap) ++i;
                if (i == lines.size()) break;
            }
            starts.push_back(i);
            used = 0;
        }
        used += lines[i].gap ? gap_h : line_h;
    }
    return starts;
}

size_t fitPrefix(const std::string& s, int avail, int suffix_w, const Measure& measure)
{
    if (measure(s.c_str(), static_cast<int>(s.size())) <= avail) return s.size();
    size_t n = s.size();
    while (n > 0) {
        --n;
        while (n > 0 && (static_cast<unsigned char>(s[n]) & 0xC0) == 0x80) --n;
        if (measure(s.c_str(), static_cast<int>(n)) + suffix_w <= avail) break;
    }
    return n;
}

}  // namespace ui::text
