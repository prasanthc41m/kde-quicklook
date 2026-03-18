#include <QApplication>
#include <QDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QPixmap>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileInfo>
#include <QTextEdit>
#include <QMimeDatabase>
#include <QMimeType>
#include <QImageReader>
#include <QFile>
#include <QMovie>
#include <QDateTime>
#include <QProcess>
#include <QTemporaryFile>
#include <QDir>
#include <QScrollArea>
#include <QPushButton>
#include <QSlider>
#include <QStyle>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QAudioOutput>
#include <poppler/qt6/poppler-qt6.h>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/tstring.h>
#include <taglib/id3v2tag.h>
#include <taglib/id3v2frame.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/mpegfile.h>
#include <taglib/flacfile.h>
#include <taglib/flacpicture.h>

// ─── Metadata bar ────────────────────────────────────────────────────────────
static QLabel* makeMetaBar(const QString &filePath, const QString &extra = QString())
{
    QFileInfo fi(filePath);
    qint64 bytes = fi.size();
    QString size;
    if (bytes < 1024)
        size = QStringLiteral("%1 B").arg(bytes);
    else if (bytes < 1024 * 1024)
        size = QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    else
        size = QStringLiteral("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 2);

    QString modified = fi.lastModified().toString(QStringLiteral("yyyy-MM-dd  hh:mm:ss"));
    QString text = QStringLiteral("  📄 %1   💾 %2   🕒 %3")
                       .arg(fi.fileName(), size, modified);
    if (!extra.isEmpty())
        text += QStringLiteral("   %1").arg(extra);

    auto *bar = new QLabel(text);
    bar->setStyleSheet(QStringLiteral(
        "background:#1e1e1e; color:#cccccc; padding:4px 8px; font-size:12px;"));
    bar->setFixedHeight(28);
    return bar;
}

// ─── Video player widget ──────────────────────────────────────────────────────
// ─── Audio player widget ──────────────────────────────────────────────────────
class AudioPlayer : public QWidget {
    Q_OBJECT
public:
    AudioPlayer(const QString &filePath, QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setStyleSheet(QStringLiteral("background:#1a1a2e;"));
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(40, 30, 40, 30);
        root->setSpacing(16);

        QString title  = QFileInfo(filePath).baseName();
        QString artist = QStringLiteral("Unknown Artist");
        QString album  = QStringLiteral("Unknown Album");
        QPixmap albumArt;

        TagLib::FileRef f(filePath.toUtf8().constData());
        if (!f.isNull() && f.tag()) {
            auto *tag = f.tag();
            if (!tag->title().isEmpty())
                title  = QString::fromStdWString(tag->title().toWString());
            if (!tag->artist().isEmpty())
                artist = QString::fromStdWString(tag->artist().toWString());
            if (!tag->album().isEmpty())
                album  = QString::fromStdWString(tag->album().toWString());
        }

        auto *mpegFile = dynamic_cast<TagLib::MPEG::File*>(f.file());
        if (mpegFile && mpegFile->ID3v2Tag()) {
            auto frames = mpegFile->ID3v2Tag()->frameListMap()["APIC"];
            if (!frames.isEmpty()) {
                auto *pic = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame*>(frames.front());
                if (pic) {
                    QPixmap px;
                    px.loadFromData(
                        reinterpret_cast<const uchar*>(pic->picture().data()),
                        pic->picture().size());
                    if (!px.isNull()) albumArt = px;
                }
            }
        }

        auto *flacFile = dynamic_cast<TagLib::FLAC::File*>(f.file());
        if (flacFile && !flacFile->pictureList().isEmpty()) {
            auto *pic = flacFile->pictureList().front();
            QPixmap px;
            px.loadFromData(
                reinterpret_cast<const uchar*>(pic->data().data()),
                pic->data().size());
            if (!px.isNull()) albumArt = px;
        }

        auto *artLabel = new QLabel(this);
        artLabel->setAlignment(Qt::AlignCenter);
        artLabel->setFixedSize(220, 220);
        if (!albumArt.isNull()) {
            artLabel->setPixmap(albumArt.scaled(220, 220,
                Qt::KeepAspectRatio, Qt::SmoothTransformation));
            artLabel->setStyleSheet(QStringLiteral("border-radius:8px; background:#000;"));
        } else {
            artLabel->setText(QStringLiteral("🎵"));
            artLabel->setStyleSheet(QStringLiteral(
                "font-size:80px; background:#2d2d44; border-radius:8px;"));
        }

        auto *artRow = new QHBoxLayout();
        artRow->addStretch();
        artRow->addWidget(artLabel);
        artRow->addStretch();
        root->addLayout(artRow);

        auto *titleLabel = new QLabel(title, this);
        titleLabel->setAlignment(Qt::AlignCenter);
        titleLabel->setStyleSheet(QStringLiteral("color:white; font-size:18px; font-weight:bold;"));
        titleLabel->setWordWrap(true);
        root->addWidget(titleLabel);

        auto *artistLabel = new QLabel(artist + QStringLiteral(" — ") + album, this);
        artistLabel->setAlignment(Qt::AlignCenter);
        artistLabel->setStyleSheet(QStringLiteral("color:#aaaacc; font-size:13px;"));
        artistLabel->setWordWrap(true);
        root->addWidget(artistLabel);

        m_seek = new QSlider(Qt::Horizontal, this);
        m_seek->setRange(0, 0);
        m_seek->setStyleSheet(QStringLiteral(
            "QSlider::groove:horizontal { height:4px; background:#444; border-radius:2px; }"
            "QSlider::handle:horizontal { width:14px; height:14px; margin:-5px 0;"
            " background:white; border-radius:7px; }"
            "QSlider::sub-page:horizontal { background:#7c6af7; border-radius:2px; }"));
        root->addWidget(m_seek);

        auto *timeRow = new QHBoxLayout();
        m_posLabel = new QLabel(QStringLiteral("0:00"), this);
        m_posLabel->setStyleSheet(QStringLiteral("color:#888; font-size:11px;"));
        m_durLabel = new QLabel(QStringLiteral("0:00"), this);
        m_durLabel->setStyleSheet(QStringLiteral("color:#888; font-size:11px;"));
        timeRow->addWidget(m_posLabel);
        timeRow->addStretch();
        timeRow->addWidget(m_durLabel);
        root->addLayout(timeRow);

        auto *ctrlRow = new QHBoxLayout();
        ctrlRow->setSpacing(16);
        auto btnStyle = QStringLiteral(
            "QPushButton { background:#2d2d44; color:white; border-radius:20px;"
            " font-size:18px; border:none; }"
            "QPushButton:hover { background:#7c6af7; }");
        m_playBtn = new QPushButton(QStringLiteral("▶"), this);
        m_playBtn->setFixedSize(48, 48);
        m_playBtn->setStyleSheet(btnStyle);
        auto *volIcon = new QLabel(QStringLiteral("🔊"), this);
        volIcon->setStyleSheet(QStringLiteral("color:white; font-size:16px;"));
        m_vol = new QSlider(Qt::Horizontal, this);
        m_vol->setRange(0, 100);
        m_vol->setValue(80);
        m_vol->setFixedWidth(80);
        m_vol->setStyleSheet(m_seek->styleSheet());
        ctrlRow->addStretch();
        ctrlRow->addWidget(m_playBtn);
        ctrlRow->addStretch();
        ctrlRow->addWidget(volIcon);
        ctrlRow->addWidget(m_vol);
        root->addLayout(ctrlRow);

        m_player = new QMediaPlayer(this);
        m_audio  = new QAudioOutput(this);
        m_player->setAudioOutput(m_audio);
        m_audio->setVolume(0.8f);
        m_player->setSource(QUrl::fromLocalFile(filePath));

        connect(m_playBtn, &QPushButton::clicked, this, &AudioPlayer::togglePlay);
        connect(m_seek, &QSlider::sliderMoved, m_player, &QMediaPlayer::setPosition);
        connect(m_vol, &QSlider::valueChanged, this, [this](int v){
            m_audio->setVolume(v / 100.0f);
        });
        connect(m_player, &QMediaPlayer::durationChanged, this, [this](qint64 dur){
            m_seek->setRange(0, (int)dur);
            m_duration = dur;
            m_durLabel->setText(formatTime(dur));
        });
        connect(m_player, &QMediaPlayer::positionChanged, this, [this](qint64 pos){
            if (!m_seek->isSliderDown()) m_seek->setValue((int)pos);
            m_posLabel->setText(formatTime(pos));
        });
        connect(m_player, &QMediaPlayer::playbackStateChanged, this,
            [this](QMediaPlayer::PlaybackState s){
                m_playBtn->setText(s == QMediaPlayer::PlayingState
                    ? QStringLiteral("⏸") : QStringLiteral("▶"));
            });
        m_player->play();
    }

    void togglePlay() {
        if (m_player->playbackState() == QMediaPlayer::PlayingState)
            m_player->pause();
        else
            m_player->play();
    }

    void stopPlayback() { m_player->stop(); }

private:
    QString formatTime(qint64 ms) {
        int s = ms / 1000;
        return QStringLiteral("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QLatin1Char('0'));
    }
    QMediaPlayer *m_player   = nullptr;
    QAudioOutput *m_audio    = nullptr;
    QPushButton  *m_playBtn  = nullptr;
    QSlider      *m_seek     = nullptr;
    QSlider      *m_vol      = nullptr;
    QLabel       *m_posLabel = nullptr;
    QLabel       *m_durLabel = nullptr;
    qint64        m_duration = 0;
};

class VideoPlayer : public QWidget {
    Q_OBJECT
public:
    VideoPlayer(const QString &filePath, QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setStyleSheet(QStringLiteral("background:#1a1a1a;"));
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        // Video surface
        m_video = new QVideoWidget(this);
        m_video->setStyleSheet(QStringLiteral("background:#000;"));
        root->addWidget(m_video, 1);

        // Controls bar
        auto *controls = new QWidget(this);
        controls->setFixedHeight(48);
        controls->setStyleSheet(QStringLiteral("background:#1e1e1e;"));
        auto *hbox = new QHBoxLayout(controls);
        hbox->setContentsMargins(8, 4, 8, 4);
        hbox->setSpacing(8);

        // Play/Pause button
        m_playBtn = new QPushButton(this);
        m_playBtn->setFixedSize(32, 32);
        m_playBtn->setText(QStringLiteral("▶"));
        m_playBtn->setStyleSheet(QStringLiteral(
            "QPushButton { background:#444; color:white; border-radius:4px;"
            " font-size:14px; border:none; }"
            "QPushButton:hover { background:#666; }"));
        hbox->addWidget(m_playBtn);

        // Seek slider
        m_seek = new QSlider(Qt::Horizontal, this);
        m_seek->setRange(0, 0);
        m_seek->setStyleSheet(QStringLiteral(
            "QSlider::groove:horizontal { height:4px; background:#444; border-radius:2px; }"
            "QSlider::handle:horizontal { width:12px; height:12px; margin:-4px 0;"
            " background:white; border-radius:6px; }"
            "QSlider::sub-page:horizontal { background:#1db954; border-radius:2px; }"));
        hbox->addWidget(m_seek, 1);

        // Time label
        m_timeLabel = new QLabel(QStringLiteral("0:00 / 0:00"), this);
        m_timeLabel->setStyleSheet(QStringLiteral("color:#aaa; font-size:11px;"));
        m_timeLabel->setFixedWidth(90);
        hbox->addWidget(m_timeLabel);

        // Volume button + slider
        m_volBtn = new QPushButton(QStringLiteral("🔊"), this);
        m_volBtn->setFixedSize(28, 28);
        m_volBtn->setStyleSheet(QStringLiteral(
            "QPushButton { background:transparent; color:white; border:none; font-size:14px; }"
            "QPushButton:hover { color:#aaa; }"));
        hbox->addWidget(m_volBtn);

        m_vol = new QSlider(Qt::Horizontal, this);
        m_vol->setRange(0, 100);
        m_vol->setValue(80);
        m_vol->setFixedWidth(70);
        m_vol->setStyleSheet(m_seek->styleSheet());
        hbox->addWidget(m_vol);

        root->addWidget(controls);

        // Media player setup
        m_player = new QMediaPlayer(this);
        m_audio  = new QAudioOutput(this);
        m_player->setAudioOutput(m_audio);
        m_player->setVideoOutput(m_video);
        m_audio->setVolume(0.8f);
        m_player->setSource(QUrl::fromLocalFile(filePath));

        // Connections
        connect(m_playBtn, &QPushButton::clicked, this, &VideoPlayer::togglePlay);
        connect(m_seek, &QSlider::sliderMoved, this, [this](int v){
            m_player->setPosition(v);
        });
        connect(m_vol, &QSlider::valueChanged, this, [this](int v){
            m_audio->setVolume(v / 100.0f);
            m_volBtn->setText(v == 0 ? QStringLiteral("🔇") : QStringLiteral("🔊"));
        });
        connect(m_volBtn, &QPushButton::clicked, this, [this](){
            bool muted = m_audio->volume() > 0;
            m_audio->setVolume(muted ? 0.0f : 0.8f);
            m_vol->setValue(muted ? 0 : 80);
        });
        connect(m_player, &QMediaPlayer::durationChanged, this, [this](qint64 dur){
            m_seek->setRange(0, (int)dur);
            m_duration = dur;
            updateTimeLabel(m_player->position());
        });
        connect(m_player, &QMediaPlayer::positionChanged, this, [this](qint64 pos){
            if (!m_seek->isSliderDown())
                m_seek->setValue((int)pos);
            updateTimeLabel(pos);
        });
        connect(m_player, &QMediaPlayer::playbackStateChanged, this,
            [this](QMediaPlayer::PlaybackState s){
                m_playBtn->setText(s == QMediaPlayer::PlayingState
                    ? QStringLiteral("⏸") : QStringLiteral("▶"));
            });

        // Auto-play
        m_player->play();
    }

    void togglePlay() {
        if (m_player->playbackState() == QMediaPlayer::PlayingState)
            m_player->pause();
        else
            m_player->play();
    }

    void stopPlayback() { m_player->stop(); }

private:
    void updateTimeLabel(qint64 pos) {
        auto fmt = [](qint64 ms) {
            int s = ms / 1000;
            return QStringLiteral("%1:%2")
                .arg(s / 60)
                .arg(s % 60, 2, 10, QLatin1Char('0'));
        };
        m_timeLabel->setText(fmt(pos) + QStringLiteral(" / ") + fmt(m_duration));
    }

    QMediaPlayer  *m_player  = nullptr;
    QAudioOutput  *m_audio   = nullptr;
    QVideoWidget  *m_video   = nullptr;
    QPushButton   *m_playBtn = nullptr;
    QSlider       *m_seek    = nullptr;
    QSlider       *m_vol     = nullptr;
    QPushButton   *m_volBtn  = nullptr;
    QLabel        *m_timeLabel = nullptr;
    qint64         m_duration  = 0;
};

// ─── Main window ─────────────────────────────────────────────────────────────
class QuickLookWindow : public QDialog {
    Q_OBJECT
public:
    QuickLookWindow(const QString &filePath, QWidget *parent = nullptr)
        : QDialog(parent, Qt::Window)
    {
        setWindowTitle(QFileInfo(filePath).fileName());
        resize(900, 680);
        setStyleSheet(QStringLiteral("background:#2b2b2b;"));

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        QMimeDatabase db;
        QMimeType mime = db.mimeTypeForFile(filePath);
        const QString mimeName = mime.name();

        // ── Animated GIF ──────────────────────────────────────────────────
        if (mimeName == QStringLiteral("image/gif")) {
            auto *label = new QLabel(this);
            label->setAlignment(Qt::AlignCenter);
            label->setStyleSheet(QStringLiteral("background:#2b2b2b;"));
            auto *movie = new QMovie(filePath, QByteArray(), this);
            movie->setScaledSize(QSize(860, 600));
            label->setMovie(movie);
            movie->start();
            root->addWidget(label, 1);
            root->addWidget(makeMetaBar(filePath,
                QStringLiteral("🎞 GIF  %1 frames").arg(movie->frameCount())));

        // ── Images ────────────────────────────────────────────────────────
        } else if (mimeName.startsWith(QStringLiteral("image/"))) {
            auto *label = new QLabel(this);
            label->setAlignment(Qt::AlignCenter);
            label->setStyleSheet(QStringLiteral("background:#2b2b2b;"));
            QImageReader reader(filePath);
            QImage image = reader.read();
            QString extra;
            if (!image.isNull()) {
                extra = QStringLiteral("📐 %1 × %2 px")
                            .arg(image.width()).arg(image.height());
                label->setPixmap(QPixmap::fromImage(image).scaled(
                    860, 600, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            } else {
                label->setText(QStringLiteral("Cannot load image."));
            }
            root->addWidget(label, 1);
            root->addWidget(makeMetaBar(filePath, extra));

        // ── PDF ───────────────────────────────────────────────────────────
        } else if (mimeName == QStringLiteral("application/pdf")) {
            auto *scroll = new QScrollArea(this);
            scroll->setStyleSheet(QStringLiteral("background:#2b2b2b; border:none;"));
            scroll->setWidgetResizable(true);
            auto *container = new QWidget();
            container->setStyleSheet(QStringLiteral("background:#2b2b2b;"));
            auto *vbox = new QVBoxLayout(container);
            vbox->setSpacing(8);
            vbox->setContentsMargins(16, 16, 16, 16);
            std::unique_ptr<Poppler::Document> doc(Poppler::Document::load(filePath));
            QString extra;
            if (doc && !doc->isLocked()) {
                doc->setRenderHint(Poppler::Document::Antialiasing);
                doc->setRenderHint(Poppler::Document::TextAntialiasing);
                int pages = doc->numPages();
                extra = QStringLiteral("📑 %1 page%2")
                            .arg(pages).arg(pages == 1 ? "" : "s");
                for (int i = 0; i < pages; ++i) {
                    std::unique_ptr<Poppler::Page> page(doc->page(i));
                    if (!page) continue;
                    QImage img = page->renderToImage(150, 150);
                    if (img.isNull()) continue;
                    auto *lbl = new QLabel();
                    lbl->setAlignment(Qt::AlignCenter);
                    lbl->setPixmap(QPixmap::fromImage(img).scaledToWidth(
                        820, Qt::SmoothTransformation));
                    vbox->addWidget(lbl);
                }
            } else {
                auto *err = new QLabel(QStringLiteral("Cannot open PDF."));
                err->setStyleSheet(QStringLiteral("color:white;"));
                err->setAlignment(Qt::AlignCenter);
                vbox->addWidget(err);
            }
            scroll->setWidget(container);
            root->addWidget(scroll, 1);
            root->addWidget(makeMetaBar(filePath, extra));

        // ── Video ─────────────────────────────────────────────────────────
        // ── Audio ─────────────────────────────────────────────────────────
        } else if (mimeName.startsWith(QStringLiteral("audio/"))) {
            m_audioPlayer = new AudioPlayer(filePath, this);
            root->addWidget(m_audioPlayer, 1);
            root->addWidget(makeMetaBar(filePath,
                QStringLiteral("🎵 %1").arg(mimeName)));

        } else if (mimeName.startsWith(QStringLiteral("video/"))) {
            m_videoPlayer = new VideoPlayer(filePath, this);
            root->addWidget(m_videoPlayer, 1);
            root->addWidget(makeMetaBar(filePath,
                QStringLiteral("🎬 %1").arg(mimeName)));

        // ── Text ──────────────────────────────────────────────────────────
        } else if (mimeName.startsWith(QStringLiteral("text/")) ||
                   mime.inherits(QStringLiteral("text/plain"))) {
            auto *textEdit = new QTextEdit(this);
            textEdit->setReadOnly(true);
            textEdit->setStyleSheet(QStringLiteral(
                "background:#1e1e1e; color:#dcdcdc; font-family:monospace;"
                " font-size:13px; border:none;"));
            QFile f(filePath);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text))
                textEdit->setPlainText(QString::fromUtf8(f.readAll()));
            else
                textEdit->setPlainText(QStringLiteral("Cannot read file."));
            root->addWidget(textEdit, 1);
            root->addWidget(makeMetaBar(filePath));

        // ── Unsupported ───────────────────────────────────────────────────
        } else {
            auto *label = new QLabel(this);
            label->setAlignment(Qt::AlignCenter);
            label->setStyleSheet(QStringLiteral("color:#aaaaaa; font-size:16px;"));
            label->setText(
                QStringLiteral("No preview available\n\n%1\n\nType: %2")
                    .arg(QFileInfo(filePath).fileName(), mimeName));
            root->addWidget(label, 1);
            root->addWidget(makeMetaBar(filePath));
        }
    }

    void closeEvent(QCloseEvent *event) override {
        if (m_videoPlayer) m_videoPlayer->stopPlayback();
        if (m_audioPlayer) m_audioPlayer->stopPlayback();
        QDialog::closeEvent(event);
    }

    void keyPressEvent(QKeyEvent *event) override {
        if (event->key() == Qt::Key_Space) {
            if (m_videoPlayer)
                m_videoPlayer->togglePlay();
            else
                close();
        } else if (event->key() == Qt::Key_Escape) {
            close();
        } else {
            QDialog::keyPressEvent(event);
        }
    }

private:
    VideoPlayer *m_videoPlayer = nullptr;
    AudioPlayer *m_audioPlayer = nullptr;
};

#include "quicklook.moc"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    if (argc < 2) {
        qWarning("Usage: kde-quicklook <file>");
        return 1;
    }
    QString filePath = QString::fromLocal8Bit(argv[1]);
    QuickLookWindow w(filePath);
    w.show();
    return app.exec();
}
