#include <QImage>

struct quirc;

class QrDecoder
{
public:
    QrDecoder(const QrDecoder &) = delete;
    QrDecoder &operator=(const QrDecoder &) = delete;

    QrDecoder();
    ~QrDecoder();

    QString decode(const QImage &image);

private:
    quirc *m_qr;
};
