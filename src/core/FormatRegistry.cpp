#include "FormatRegistry.h"
#include <QFileInfo>

namespace uc::core {

FormatRegistry& FormatRegistry::instance() {
    static FormatRegistry s_instance;
    return s_instance;
}

FormatRegistry::FormatRegistry() {
    initializeFormats();
}

void FormatRegistry::initializeFormats() {
    m_formats.clear();
    m_idToIndex.clear();
    m_extToIndex.clear();
    m_aliasToNormalized.clear();

    // Extension Normalization Aliases (audited from app/src/lib/types/file.svelte.ts)
    m_aliasToNormalized["jpg"] = "jpeg";
    m_aliasToNormalized["jfif"] = "jpeg";
    m_aliasToNormalized["jpe"] = "jpeg";
    m_aliasToNormalized["tif"] = "tiff";
    m_aliasToNormalized["ogx"] = "ogv";
    m_aliasToNormalized["asf"] = "wmv";
    m_aliasToNormalized["mpg"] = "mpeg";
    m_aliasToNormalized["mpe"] = "mpeg";
    m_aliasToNormalized["mpv"] = "mpeg";
    m_aliasToNormalized["markdown"] = "md";
    m_aliasToNormalized["htm"] = "html";
    m_aliasToNormalized["tex"] = "latex";

    auto addFmt = [this](const QString& id, const QString& name, MediaCategory cat,
                         const QString& defaultExt, const QStringList& exts,
                         const QStringList& mimes, bool canIn = true, bool canOut = true,
                         CapabilityTier tier = CapabilityTier::TierA_VerifiedVertAndNative) {
        FormatDefinition def{id, name, cat, defaultExt, exts, mimes, canIn, canOut, tier};
        const int index = static_cast<int>(m_formats.size());
        m_formats.append(def);
        m_idToIndex[id.toLower()] = index;
        for (const QString& ext : exts) {
            m_extToIndex[ext.toLower()] = index;
        }
    };

    // ==========================================
    // --- 1. Video Formats (Engine: FFmpeg) ---
    // ==========================================
    addFmt("mp4", "MP4 Video", MediaCategory::Video, "mp4", {"mp4", "m4v"}, {"video/mp4"});
    addFmt("webm", "WebM Video", MediaCategory::Video, "webm", {"webm"}, {"video/webm"});
    addFmt("mkv", "Matroska Video", MediaCategory::Video, "mkv", {"mkv"}, {"video/x-matroska"});
    addFmt("mov", "QuickTime Video", MediaCategory::Video, "mov", {"mov", "qt"}, {"video/quicktime"});
    addFmt("avi", "Audio Video Interleave", MediaCategory::Video, "avi", {"avi"}, {"video/x-msvideo"});
    addFmt("flv", "Flash Video", MediaCategory::Video, "flv", {"flv"}, {"video/x-flv"});
    addFmt("wmv", "Windows Media Video", MediaCategory::Video, "wmv", {"wmv", "asf"}, {"video/x-ms-wmv"});
    addFmt("ts", "MPEG Transport Stream", MediaCategory::Video, "ts", {"ts", "mts", "m2ts"}, {"video/mp2t"});
    addFmt("ogv", "Ogg Theora Video", MediaCategory::Video, "ogv", {"ogv", "ogx"}, {"video/ogg"});
    addFmt("3gp", "3GPP Mobile Video", MediaCategory::Video, "3gp", {"3gp", "3g2"}, {"video/3gpp"});
    addFmt("mpeg", "MPEG-1/2 Video", MediaCategory::Video, "mpeg", {"mpeg", "mpg", "mpe"}, {"video/mpeg"});
    addFmt("vob", "DVD Video Object", MediaCategory::Video, "vob", {"vob"}, {"video/dvd"});
    addFmt("mxf", "Material Exchange Format", MediaCategory::Video, "mxf", {"mxf"}, {"application/mxf"});
    addFmt("gxf", "General Exchange Format", MediaCategory::Video, "gxf", {"gxf"}, {"application/gxf"});
    addFmt("nut", "NUT Container", MediaCategory::Video, "nut", {"nut"}, {"video/x-nut"});
    addFmt("amv", "Actions Media Video", MediaCategory::Video, "amv", {"amv"}, {"video/x-amv"});
    addFmt("y4m", "YUV4MPEG2 Video", MediaCategory::Video, "y4m", {"y4m"}, {"video/x-raw-yuv"});
    addFmt("dv", "Digital Video", MediaCategory::Video, "dv", {"dv", "dif"}, {"video/x-dv"});
    addFmt("wtv", "Windows Recorded TV Show", MediaCategory::Video, "wtv", {"wtv"}, {"video/x-wtv"});
    // RealMedia is input-only in FFmpeg/VERT
    addFmt("rm", "RealMedia Video", MediaCategory::Video, "rm", {"rm", "rmvb"}, {"application/vnd.rn-realmedia"}, true, false, CapabilityTier::TierD_UnsupportedOrRestricted);

    // ==========================================
    // --- 2. Audio Formats (Engine: FFmpeg) ---
    // ==========================================
    addFmt("mp3", "MP3 Audio", MediaCategory::Audio, "mp3", {"mp3"}, {"audio/mpeg"});
    addFmt("wav", "WAV Audio", MediaCategory::Audio, "wav", {"wav"}, {"audio/wav", "audio/x-wav"});
    addFmt("flac", "FLAC Audio", MediaCategory::Audio, "flac", {"flac"}, {"audio/flac"});
    addFmt("aac", "AAC Audio", MediaCategory::Audio, "aac", {"aac"}, {"audio/aac"});
    addFmt("ogg", "Ogg Vorbis Audio", MediaCategory::Audio, "ogg", {"ogg", "oga"}, {"audio/ogg"});
    addFmt("opus", "Opus Audio", MediaCategory::Audio, "opus", {"opus"}, {"audio/opus"});
    addFmt("m4a", "M4A Audio", MediaCategory::Audio, "m4a", {"m4a", "alac"}, {"audio/mp4", "audio/x-m4a"});
    addFmt("wma", "Windows Media Audio", MediaCategory::Audio, "wma", {"wma"}, {"audio/x-ms-wma"});
    addFmt("aiff", "AIFF Audio", MediaCategory::Audio, "aiff", {"aiff", "aif", "aifc"}, {"audio/aiff"});
    addFmt("ac3", "Dolby Digital AC-3", MediaCategory::Audio, "ac3", {"ac3"}, {"audio/ac3"});
    addFmt("eac3", "Enhanced AC-3 (Dolby Digital Plus)", MediaCategory::Audio, "eac3", {"eac3"}, {"audio/eac3"});
    addFmt("dts", "DTS Audio", MediaCategory::Audio, "dts", {"dts"}, {"audio/vnd.dts"});
    addFmt("mp2", "MPEG-1 Audio Layer 2", MediaCategory::Audio, "mp2", {"mp2"}, {"audio/x-mp2"});
    addFmt("au", "Sun/NeXT Audio", MediaCategory::Audio, "au", {"au", "snd"}, {"audio/basic"});
    addFmt("m4b", "M4B Audiobook", MediaCategory::Audio, "m4b", {"m4b"}, {"audio/m4b"});
    addFmt("voc", "Creative Voice", MediaCategory::Audio, "voc", {"voc"}, {"audio/x-voc"});
    addFmt("wv", "WavPack Lossless Audio", MediaCategory::Audio, "wv", {"wv"}, {"audio/x-wavpack"});
    addFmt("qoa", "Quite OK Audio", MediaCategory::Audio, "qoa", {"qoa"}, {"audio/qoa"});
    // Read-only audio formats
    addFmt("caf", "Core Audio Format", MediaCategory::Audio, "caf", {"caf"}, {"audio/x-caf"}, true, false, CapabilityTier::TierD_UnsupportedOrRestricted);
    addFmt("mogg", "Multitrack Ogg", MediaCategory::Audio, "mogg", {"mogg"}, {"audio/ogg"}, true, false, CapabilityTier::TierD_UnsupportedOrRestricted);
    addFmt("mp1", "MPEG-1 Audio Layer 1", MediaCategory::Audio, "mp1", {"mp1"}, {"audio/x-mp1"}, true, false, CapabilityTier::TierD_UnsupportedOrRestricted);
    addFmt("mpc", "Musepack Audio", MediaCategory::Audio, "mpc", {"mpc"}, {"audio/x-musepack"}, true, false, CapabilityTier::TierD_UnsupportedOrRestricted);
    addFmt("dsd", "Direct Stream Digital", MediaCategory::Audio, "dsd", {"dsd", "dsf", "dff"}, {"audio/x-dsd"}, true, false, CapabilityTier::TierD_UnsupportedOrRestricted);

    // ===============================================
    // --- 3. Image Formats (Engine: ImageMagick) ---
    // ===============================================
    addFmt("png", "PNG Image", MediaCategory::Image, "png", {"png"}, {"image/png"});
    addFmt("jpeg", "JPEG Image", MediaCategory::Image, "jpeg", {"jpeg", "jpg", "jfif", "jpe"}, {"image/jpeg"});
    addFmt("webp", "WebP Image", MediaCategory::Image, "webp", {"webp"}, {"image/webp"});
    addFmt("gif", "GIF Image", MediaCategory::Image, "gif", {"gif"}, {"image/gif"});
    addFmt("avif", "AVIF Image", MediaCategory::Image, "avif", {"avif"}, {"image/avif"});
    addFmt("bmp", "Bitmap Image", MediaCategory::Image, "bmp", {"bmp"}, {"image/bmp"});
    addFmt("tiff", "TIFF Image", MediaCategory::Image, "tiff", {"tiff", "tif"}, {"image/tiff"});
    addFmt("ico", "Windows Icon", MediaCategory::Image, "ico", {"ico"}, {"image/x-icon"});
    addFmt("svg", "Scalable Vector Graphics", MediaCategory::Image, "svg", {"svg"}, {"image/svg+xml"});
    addFmt("psd", "Adobe Photoshop Document", MediaCategory::Image, "psd", {"psd"}, {"image/vnd.adobe.photoshop"});
    addFmt("tga", "Truevision TGA", MediaCategory::Image, "tga", {"tga"}, {"image/x-tga"});
    addFmt("heic", "High Efficiency Image", MediaCategory::Image, "heic", {"heic", "heif"}, {"image/heic"});
    addFmt("cur", "Windows Cursor", MediaCategory::Image, "cur", {"cur"}, {"image/x-icon"});
    addFmt("hdr", "Radiance RGBE HDR", MediaCategory::Image, "hdr", {"hdr"}, {"image/vnd.radiance"});
    addFmt("jxl", "JPEG XL Image", MediaCategory::Image, "jxl", {"jxl"}, {"image/jxl"});
    addFmt("exr", "OpenEXR High Dynamic Range", MediaCategory::Image, "exr", {"exr"}, {"image/x-exr"});
    addFmt("pcx", "ZSoft PCX Image", MediaCategory::Image, "pcx", {"pcx"}, {"image/x-pcx"});
    addFmt("pbm", "Portable Bitmap", MediaCategory::Image, "pbm", {"pbm"}, {"image/x-portable-bitmap"});
    addFmt("pgm", "Portable Graymap", MediaCategory::Image, "pgm", {"pgm"}, {"image/x-portable-graymap"});
    addFmt("ppm", "Portable Pixmap", MediaCategory::Image, "ppm", {"ppm"}, {"image/x-portable-pixmap"});
    addFmt("pnm", "Portable Anymap", MediaCategory::Image, "pnm", {"pnm"}, {"image/x-portable-anymap"});
    addFmt("xpm", "X Pixmap", MediaCategory::Image, "xpm", {"xpm"}, {"image/x-xpixmap"});
    addFmt("xbm", "X Bitmap", MediaCategory::Image, "xbm", {"xbm"}, {"image/x-xbitmap"});
    addFmt("wbmp", "Wireless Bitmap", MediaCategory::Image, "wbmp", {"wbmp"}, {"image/vnd.wap.wbmp"});
    addFmt("dds", "DirectDraw Surface", MediaCategory::Image, "dds", {"dds"}, {"image/x-dds"});
    addFmt("dpx", "Digital Picture Exchange", MediaCategory::Image, "dpx", {"dpx"}, {"image/x-dpx"});
    addFmt("cin", "Cineon Image", MediaCategory::Image, "cin", {"cin"}, {"image/x-cineon"});
    addFmt("sgi", "Silicon Graphics Image", MediaCategory::Image, "sgi", {"sgi", "rgb", "rgba"}, {"image/sgi"});
    addFmt("ras", "Sun Raster Image", MediaCategory::Image, "ras", {"ras", "sun"}, {"image/x-sun-raster"});
    addFmt("qoi", "Quite OK Image", MediaCategory::Image, "qoi", {"qoi"}, {"image/qoi"});
    addFmt("mpo", "Multi-Picture Object", MediaCategory::Image, "mpo", {"mpo"}, {"image/mpo"}, true, false);
    // Camera RAW formats (strictly input-only)
    const QStringList rawExts = {"cr2", "crw", "cr3", "nef", "arw", "dng", "rw2", "raf", "orf", "pef", "dcr", "nrw", "mos", "raw", "3fr", "erf", "mrw", "mef", "srw", "sr2", "srf"};
    for (const auto& rawExt : rawExts) {
        addFmt(rawExt, rawExt.toUpper() + " Camera RAW", MediaCategory::Image, rawExt, {rawExt}, {"image/x-dcraw"}, true, false, CapabilityTier::TierB_NativeExpanded);
    }
    // Medical & Special formats
    addFmt("dcm", "DICOM Medical Image", MediaCategory::Image, "dcm", {"dcm", "dicom"}, {"application/dicom"}, true, false, CapabilityTier::TierB_NativeExpanded);
    addFmt("icns", "Apple Icon Image", MediaCategory::Image, "icns", {"icns"}, {"image/x-icns"}, true, false, CapabilityTier::TierB_NativeExpanded);
    addFmt("xcf", "GIMP Document", MediaCategory::Image, "xcf", {"xcf"}, {"image/x-xcf"}, true, false, CapabilityTier::TierB_NativeExpanded);

    // ==========================================
    // --- 4. Document Formats (Engine: Pandoc) ---
    // ==========================================
    addFmt("md", "Markdown Document", MediaCategory::Document, "md", {"md", "markdown"}, {"text/markdown"});
    addFmt("html", "HTML Document", MediaCategory::Document, "html", {"html", "htm"}, {"text/html"});
    addFmt("docx", "Microsoft Word Document", MediaCategory::Document, "docx", {"docx"}, {"application/vnd.openxmlformats-officedocument.wordprocessingml.document"});
    addFmt("odt", "OpenDocument Text", MediaCategory::Document, "odt", {"odt"}, {"application/vnd.oasis.opendocument.text"});
    addFmt("epub", "EPUB E-Book", MediaCategory::Document, "epub", {"epub"}, {"application/epub+zip"});
    addFmt("latex", "LaTeX Document", MediaCategory::Document, "tex", {"tex", "latex"}, {"application/x-latex"});
    addFmt("rst", "reStructuredText", MediaCategory::Document, "rst", {"rst"}, {"text/x-rst"});
    addFmt("asciidoc", "AsciiDoc Document", MediaCategory::Document, "adoc", {"adoc", "asciidoc"}, {"text/asciidoc"});
    addFmt("org", "Org-Mode Document", MediaCategory::Document, "org", {"org"}, {"text/x-org"});
    addFmt("textile", "Textile Document", MediaCategory::Document, "textile", {"textile"}, {"text/x-textile"});
    addFmt("mediawiki", "MediaWiki Markup", MediaCategory::Document, "wiki", {"wiki", "mediawiki"}, {"text/x-wiki"});
    addFmt("opml", "OPML Outline", MediaCategory::Document, "opml", {"opml"}, {"text/x-opml"});
    addFmt("rtf", "Rich Text Format", MediaCategory::Document, "rtf", {"rtf"}, {"application/rtf"}, true, false, CapabilityTier::TierD_UnsupportedOrRestricted);
    addFmt("txt", "Plain Text", MediaCategory::Document, "txt", {"txt"}, {"text/plain"});
    // PDF is output-only: Pandoc can generate PDFs via Typst/LaTeX engines, but cannot parse PDF as an input document source
    addFmt("pdf", "Portable Document Format", MediaCategory::Document, "pdf", {"pdf"}, {"application/pdf"}, false, true, CapabilityTier::TierA_VerifiedVertAndNative);
    // Explicitly unsupported in VERT
    addFmt("doc", "Legacy Word 97-2003", MediaCategory::Document, "doc", {"doc"}, {"application/msword"}, false, false, CapabilityTier::TierD_UnsupportedOrRestricted);

    // ==========================================
    // --- 5. Subtitle Formats (Engine: FFmpeg) ---
    // ==========================================
    addFmt("srt", "SubRip Subtitle", MediaCategory::Subtitle, "srt", {"srt"}, {"application/x-subrip", "text/plain"});
    addFmt("vtt", "WebVTT Subtitle", MediaCategory::Subtitle, "vtt", {"vtt", "webvtt"}, {"text/vtt"});
    addFmt("ass", "Advanced SubStation Alpha", MediaCategory::Subtitle, "ass", {"ass"}, {"text/x-ssa"});
    addFmt("ssa", "SubStation Alpha", MediaCategory::Subtitle, "ssa", {"ssa"}, {"text/x-ssa"});
    addFmt("sub", "MicroDVD / SubViewer Subtitle", MediaCategory::Subtitle, "sub", {"sub"}, {"text/x-subviewer"}, true, false);
}

QString FormatRegistry::normalizeExtension(const QString& ext) const {
    const QString clean = ext.startsWith('.') ? ext.mid(1).toLower() : ext.toLower();
    const auto it = m_aliasToNormalized.find(clean);
    if (it != m_aliasToNormalized.end()) {
        return it.value();
    }
    return clean;
}

QString FormatRegistry::normalizeFormatId(const QString& id) const {
    return normalizeExtension(id);
}

QList<FormatDefinition> FormatRegistry::formatsForCategory(MediaCategory category) const {
    QList<FormatDefinition> results;
    for (const auto& fmt : m_formats) {
        if (fmt.category == category) {
            results.append(fmt);
        }
    }
    return results;
}

const FormatDefinition* FormatRegistry::findById(const QString& id) const {
    const QString norm = normalizeFormatId(id);
    const auto it = m_idToIndex.find(norm);
    if (it != m_idToIndex.end()) {
        return &m_formats[it.value()];
    }
    return nullptr;
}

const FormatDefinition* FormatRegistry::findByExtension(const QString& ext) const {
    const QString norm = normalizeExtension(ext);
    const auto it = m_extToIndex.find(norm);
    if (it != m_extToIndex.end()) {
        return &m_formats[it.value()];
    }
    return nullptr;
}

const FormatDefinition* FormatRegistry::findByFilePath(const QString& filePath) const {
    const QFileInfo info(filePath);
    return findByExtension(info.suffix());
}

CompatibilityResult FormatRegistry::checkCompatibility(const QString& inputFormatId, const QString& outputFormatId) const {
    CompatibilityResult res;
    const auto* in = findById(inputFormatId);
    const auto* out = findById(outputFormatId);

    if (!in) {
        res.isSupported = false;
        res.reason = QString("Input format '%1' is not recognized.").arg(inputFormatId);
        return res;
    }

    if (!out) {
        res.isSupported = false;
        res.reason = QString("Target output format '%1' is not recognized.").arg(outputFormatId);
        return res;
    }

    if (!in->canInput) {
        res.isSupported = false;
        res.reason = QString("Format '%1' cannot be read as an input source.").arg(in->name);
        return res;
    }

    if (!out->canOutput) {
        res.isSupported = false;
        res.reason = QString("Format '%1' is read-only and cannot be produced as output.").arg(out->name);
        return res;
    }

    if (in->id == out->id) {
        res.isSupported = false;
        res.reason = "Source and target formats are identical (no conversion needed).";
        return res;
    }

    // 1. Same-Category Compatibility
    if (in->category == out->category) {
        res.isSupported = true;
        res.tier = CapabilityTier::TierA_VerifiedVertAndNative;
        switch (in->category) {
            case MediaCategory::Video:
                res.responsibleEngine = "FFmpeg";
                return res;
            case MediaCategory::Audio:
                res.responsibleEngine = "FFmpeg";
                return res;
            case MediaCategory::Image:
                res.responsibleEngine = "ImageMagick";
                return res;
            case MediaCategory::Document:
                res.responsibleEngine = "Pandoc";
                return res;
            case MediaCategory::Subtitle:
                res.responsibleEngine = "FFmpeg";
                return res;
        }
    }

    // 2. Cross-Category: Video -> Audio (Audio extraction)
    if (in->category == MediaCategory::Video && out->category == MediaCategory::Audio) {
        res.isSupported = true;
        res.responsibleEngine = "FFmpeg";
        res.tier = CapabilityTier::TierA_VerifiedVertAndNative;
        return res;
    }

    // 3. Cross-Category: Audio -> Video (Black video or album cover)
    if (in->category == MediaCategory::Audio && out->category == MediaCategory::Video) {
        if (out->id == "ogv" || out->id == "ogx") {
            res.isSupported = false;
            res.reason = "Audio-to-Video bridging to Ogg Video container is explicitly restricted.";
            return res;
        }
        res.isSupported = true;
        res.responsibleEngine = "FFmpeg";
        res.tier = CapabilityTier::TierA_VerifiedVertAndNative;
        return res;
    }

    // 4. Cross-Category: Image -> Document (e.g. Image -> PDF)
    if (in->category == MediaCategory::Image && out->id == "pdf") {
        res.isSupported = true;
        res.responsibleEngine = "ImageMagick";
        res.tier = CapabilityTier::TierA_VerifiedVertAndNative;
        return res;
    }

    // 5. Cross-Category: Document -> PDF
    if (in->category == MediaCategory::Document && out->id == "pdf") {
        res.isSupported = true;
        res.responsibleEngine = "Pandoc";
        res.tier = CapabilityTier::TierC_AwaitingEngine;
        res.reason = "Document to PDF conversion requires external engine (typst/xelatex/wkhtmltopdf).";
        return res;
    }

    // 6. Cross-Category: Video -> Animated Image (GIF, WebP, AVIF)
    if (in->category == MediaCategory::Video && (out->id == "gif" || out->id == "webp" || out->id == "avif")) {
        res.isSupported = true;
        res.responsibleEngine = "FFmpeg";
        res.tier = CapabilityTier::TierA_VerifiedVertAndNative;
        return res;
    }

    // 7. Cross-Category: Animated Image (GIF, WebP, AVIF) -> Video
    if ((in->id == "gif" || in->id == "webp" || in->id == "avif") && out->category == MediaCategory::Video) {
        res.isSupported = true;
        res.responsibleEngine = "FFmpeg";
        res.tier = CapabilityTier::TierA_VerifiedVertAndNative;
        return res;
    }

    res.isSupported = false;
    res.reason = QString("Direct conversion from %1 to %2 is not supported.").arg(in->name, out->name);
    return res;
}

bool FormatRegistry::isConversionSupported(const QString& inputFormatId, const QString& outputFormatId) const {
    return checkCompatibility(inputFormatId, outputFormatId).isSupported;
}

QList<FormatDefinition> FormatRegistry::availableOutputsForInput(const QString& inputFormatId) const {
    QList<FormatDefinition> results;
    const auto* in = findById(inputFormatId);
    if (!in || !in->canInput) return results;

    for (const auto& candidate : m_formats) {
        if (candidate.canOutput && candidate.id != in->id) {
            if (isConversionSupported(in->id, candidate.id)) {
                results.append(candidate);
            }
        }
    }
    return results;
}

} // namespace uc::core
