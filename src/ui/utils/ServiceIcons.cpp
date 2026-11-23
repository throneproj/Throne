#include "include/ui/utils/ServiceIcons.h"

#include <QFont>
#include <QHash>
#include <QHelpEvent>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>
#include <QStringList>
#include <QToolTip>
#include <QTransform>
#include <QtMath>

#include "include/ui/setting/ThemeManager.hpp"

namespace {
    struct ServiceIconLayer {
        QRgb fill[3]; // one colour, or 2-3 top-to-bottom gradient stops; all zero paints with the foreground
        const char *path;
    };

    struct ServiceIconGlyph {
        const char *id;
        QRectF viewBox;
        ServiceIconLayer layers[2];
    };

    // Simple Icons (CC0-1.0); copilot: theSVG microsoft-copilot (MIT); prime_video: selfh.st/icons amazon-prime-video (CC-BY-4.0).
    constexpr ServiceIconGlyph kServiceIconGlyphs[] = {
        {"chatgpt", {0, 0, 24, 24}, {
            {{}, R"(M22.2819 9.8211a5.9847 5.9847 0 0 0-.5157-4.9108 6.0462 6.0462 0 0 0-6.5098-2.9A6.0651 6.0651 0 0 0 4.9807 4.1818a5.9847 5.9847 0 0 0-3.9977 2.9 6.0462 6.0462 0 0 0 .7427 7.0966 5.98 5.98 0 0 0 .511 4.9107 6.051 6.051 0 0 0 6.5146 2.9001A5.9847 5.9847 0 0 0 13.2599 24a6.0557 6.0557 0 0 0 5.7718-4.2058 5.9894 5.9894 0 0 0 3.9977-2.9001 6.0557 6.0557 0 0 0-.7475-7.0729zm-9.022 12.6081a4.4755 4.4755 0 0 1-2.8764-1.0408l.1419-.0804 4.7783-2.7582a.7948.7948 0 0 0 .3927-.6813v-6.7369l2.02 1.1686a.071.071 0 0 1 .038.052v5.5826a4.504 4.504 0 0 1-4.4945 4.4944zm-9.6607-4.1254a4.4708 4.4708 0 0 1-.5346-3.0137l.142.0852 4.783 2.7582a.7712.7712 0 0 0 .7806 0l5.8428-3.3685v2.3324a.0804.0804 0 0 1-.0332.0615L9.74 19.9502a4.4992 4.4992 0 0 1-6.1408-1.6464zM2.3408 7.8956a4.485 4.485 0 0 1 2.3655-1.9728V11.6a.7664.7664 0 0 0 .3879.6765l5.8144 3.3543-2.0201 1.1685a.0757.0757 0 0 1-.071 0l-4.8303-2.7865A4.504 4.504 0 0 1 2.3408 7.872zm16.5963 3.8558L13.1038 8.364 15.1192 7.2a.0757.0757 0 0 1 .071 0l4.8303 2.7913a4.4944 4.4944 0 0 1-.6765 8.1042v-5.6772a.79.79 0 0 0-.407-.667zm2.0107-3.0231l-.142-.0852-4.7735-2.7818a.7759.7759 0 0 0-.7854 0L9.409 9.2297V6.8974a.0662.0662 0 0 1 .0284-.0615l4.8303-2.7866a4.4992 4.4992 0 0 1 6.6802 4.66zM8.3065 12.863l-2.02-1.1638a.0804.0804 0 0 1-.038-.0567V6.0742a4.4992 4.4992 0 0 1 7.3757-3.4537l-.142.0805L8.704 5.459a.7948.7948 0 0 0-.3927.6813zm1.0976-2.3654l2.602-1.4998 2.6069 1.4998v2.9994l-2.5974 1.4997-2.6067-1.4997Z)"},
        }},
        {"claude", {0, 0, 24, 24}, {
            {{0xffd97757}, R"(m4.7144 15.9555 4.7174-2.6471.079-.2307-.079-.1275h-.2307l-.7893-.0486-2.6956-.0729-2.3375-.0971-2.2646-.1214-.5707-.1215-.5343-.7042.0546-.3522.4797-.3218.686.0608 1.5179.1032 2.2767.1578 1.6514.0972 2.4468.255h.3886l.0546-.1579-.1336-.0971-.1032-.0972L6.973 9.8356l-2.55-1.6879-1.3356-.9714-.7225-.4918-.3643-.4614-.1578-1.0078.6557-.7225.8803.0607.2246.0607.8925.686 1.9064 1.4754 2.4893 1.8336.3643.3035.1457-.1032.0182-.0728-.164-.2733-1.3539-2.4467-1.445-2.4893-.6435-1.032-.17-.6194c-.0607-.255-.1032-.4674-.1032-.7285L6.287.1335 6.6997 0l.9957.1336.419.3642.6192 1.4147 1.0018 2.2282 1.5543 3.0296.4553.8985.2429.8318.091.255h.1579v-.1457l.1275-1.706.2368-2.0947.2307-2.6957.0789-.7589.3764-.9107.7468-.4918.5828.2793.4797.686-.0668.4433-.2853 1.8517-.5586 2.9021-.3643 1.9429h.2125l.2429-.2429.9835-1.3053 1.6514-2.0643.7286-.8196.85-.9046.5464-.4311h1.0321l.759 1.1293-.34 1.1657-1.0625 1.3478-.8804 1.1414-1.2628 1.7-.7893 1.36.0729.1093.1882-.0183 2.8535-.607 1.5421-.2794 1.8396-.3157.8318.3886.091.3946-.3278.8075-1.967.4857-2.3072.4614-3.4364.8136-.0425.0304.0486.0607 1.5482.1457.6618.0364h1.621l3.0175.2247.7892.522.4736.6376-.079.4857-1.2142.6193-1.6393-.3886-3.825-.9107-1.3113-.3279h-.1822v.1093l1.0929 1.0686 2.0035 1.8092 2.5075 2.3314.1275.5768-.3218.4554-.34-.0486-2.2039-1.6575-.85-.7468-1.9246-1.621h-.1275v.17l.4432.6496 2.3436 3.5214.1214 1.0807-.17.3521-.6071.2125-.6679-.1214-1.3721-1.9246L14.38 17.959l-1.1414-1.9428-.1397.079-.674 7.2552-.3156.3703-.7286.2793-.6071-.4614-.3218-.7468.3218-1.4753.3886-1.9246.3157-1.53.2853-1.9004.17-.6314-.0121-.0425-.1397.0182-1.4328 1.9672-2.1796 2.9446-1.7243 1.8456-.4128.164-.7164-.3704.0667-.6618.4008-.5889 2.386-3.0357 1.4389-1.882.929-1.0868-.0062-.1579h-.0546l-6.3385 4.1164-1.1293.1457-.4857-.4554.0608-.7467.2307-.2429 1.9064-1.3114Z)"},
        }},
        {"gemini", {0, 0, 24, 24}, {
            {{0xff8e75b2}, R"(M11.04 19.32Q12 21.51 12 24q0-2.49.93-4.68.96-2.19 2.58-3.81t3.81-2.55Q21.51 12 24 12q-2.49 0-4.68-.93a12.3 12.3 0 0 1-3.81-2.58 12.3 12.3 0 0 1-2.58-3.81Q12 2.49 12 0q0 2.49-.96 4.68-.93 2.19-2.55 3.81a12.3 12.3 0 0 1-3.81 2.58Q2.49 12 0 12q2.49 0 4.68.96 2.19.93 3.81 2.55t2.55 3.81)"},
        }},
        {"copilot", {-1, 15, 513, 481}, {
            {{0xff109df2, 0xffaad445, 0xffffd400}, R"(m347.3 115.17l-5.16 20.63h-49.62c-36.77 0-68.82 25.03-77.74 60.7l-44.92 179.7H80.17c-52.13 0-90.38-48.99-77.74-99.57l45.03-180.1C59.35 48.97 102.08 15.6 151.11 15.6h118.45c52.13 0 90.38 48.99 77.74 99.57)"},
            {{0xff8c48ff, 0xfff2598a, 0xffffb152}, R"(m164.7 396.83l5.16-20.63h49.62c36.77 0 68.82-25.03 77.74-60.7l44.92-179.7h89.69c52.13 0 90.38 48.99 77.74 99.57l-45.02 180.1c-11.89 47.56-54.63 80.93-103.66 80.93H242.44c-52.13 0-90.38-48.99-77.74-99.57)"},
        }},
        {"netflix", {0, 0, 24, 24}, {
            {{0xffe50914}, R"(m5.398 0 8.348 23.602c2.346.059 4.856.398 4.856.398L10.113 0H5.398zm8.489 0v9.172l4.715 13.33V0h-4.715zM5.398 1.5V24c1.873-.225 2.81-.312 4.715-.398V14.83L5.398 1.5z)"},
        }},
        {"youtube_premium", {0, 0, 24, 24}, {
            {{0xffff0000}, R"(M23.498 6.186a3.016 3.016 0 0 0-2.122-2.136C19.505 3.545 12 3.545 12 3.545s-7.505 0-9.377.505A3.017 3.017 0 0 0 .502 6.186C0 8.07 0 12 0 12s0 3.93.502 5.814a3.016 3.016 0 0 0 2.122 2.136c1.871.505 9.376.505 9.376.505s7.505 0 9.377-.505a3.015 3.015 0 0 0 2.122-2.136C24 15.93 24 12 24 12s0-3.93-.502-5.814zM9.545 15.568V8.432L15.818 12l-6.273 3.568z)"},
        }},
        {"spotify", {0, 0, 24, 24}, {
            {{0xff1ed760}, R"(M12 0C5.4 0 0 5.4 0 12s5.4 12 12 12 12-5.4 12-12S18.66 0 12 0zm5.521 17.34c-.24.359-.66.48-1.021.24-2.82-1.74-6.36-2.101-10.561-1.141-.418.122-.779-.179-.899-.539-.12-.421.18-.78.54-.9 4.56-1.021 8.52-.6 11.64 1.32.42.18.479.659.301 1.02zm1.44-3.3c-.301.42-.841.6-1.262.3-3.239-1.98-8.159-2.58-11.939-1.38-.479.12-1.02-.12-1.14-.6-.12-.48.12-1.021.6-1.141C9.6 9.9 15 10.561 18.72 12.84c.361.181.54.78.241 1.2zm.12-3.36C15.24 8.4 8.82 8.16 5.16 9.301c-.6.179-1.2-.181-1.38-.721-.18-.601.18-1.2.72-1.381 4.26-1.26 11.28-1.02 15.721 1.621.539.3.719 1.02.419 1.56-.299.421-1.02.599-1.559.3z)"},
        }},
        {"prime_video", {0, 0, 512, 512}, {
            {{0xff1a98ff}, R"(M225.3 511.7c-47.4-3.8-84.6-13.6-121.1-32.1c-24.9-12.6-40.9-23.9-58.1-41.3c-13.4-13.5-20.2-22.4-21.5-28.6c-1.1-5 .9-8.1 5.4-8.1c2.9 0 7.7 2.7 15.8 9.1c35.4 27.6 84.9 47.6 139.8 56.5c51.2 8.3 103.2 7.3 156-3.1c17.2-3.4 32.5-7.4 48.1-12.6c11.8-3.9 19.4-7.1 31.4-13c7.9-3.9 10.4-4.8 13-5c3-.2 3.3-.2 4.9 1.6c2.5 2.7 2.4 6.2-.3 10.3c-2.5 3.7-6.9 8.2-13.4 13.4c-33.5 27-87.9 46.3-146.3 52c-10.5 1.1-44.5 1.6-53.7.9m219-31.9c-3.3-2.9-2.8-6.1 2.7-15.1c7.3-12 13.4-26.6 14.9-35.5c1.3-7.5-.6-9.9-8.9-11.3c-7.8-1.4-25.8-.4-37.2 2c-2.1.4-5.9.8-8.4.8c-4.1 0-4.7-.1-5.9-1.3c-.7-.7-1.4-2.3-1.6-3.5c-.3-1.9-.1-2.5 1.5-4.3c2.5-2.9 7.4-6.1 14.1-9.3c11.6-5.5 21.8-7.8 34.9-7.8c16.9 0 30.4 4.1 34.9 10.5c1.7 2.4 1.7 2.6 1.9 10.3c.1 5.8 0 9.3-.7 12.9c-2.2 12-8.4 25.7-15.5 34.6c-4.6 5.7-14.9 15.2-18.6 17c-4.1 2.1-5.8 2.1-8.1 0m-237.9-124c-14-2.7-25.8-16.8-30.5-36.5c-3.2-13.5-3.4-32.9-.5-46.5c3.9-18 12.6-31 24.4-36.5c15-7 30.6-2.7 39.9 11.1c1.5 2.3 3 4 3.3 3.8s.5-10.2.5-25.4c0-24.6 0-25.1 1.1-26.2s1.7-1.1 14.2-1c9.5.1 13.3.4 13.8.9c1.3 1.3 1.5 12.7 1.2 82.5l-.2 69.2l-1.2 1.3c-1.1 1.2-1.5 1.3-11.6 1.3c-5.8 0-10.9-.2-11.4-.3c-1.5-.6-2.4-3-3.7-10.1c-.7-3.8-1.4-6.8-1.6-6.8s-1.1 1.4-2 3.2c-2.4 4.8-9.1 11.1-14.3 13.7c-2.3 1.1-5.6 2.3-7.2 2.6c-3.6.5-10.4.4-14.2-.3m24.5-26.4c9.1-4.4 13.9-19 12.9-38.8c-.9-18-6.2-28.4-15.7-31c-3.1-.8-4.1-.9-6.7-.3c-9 2-14.1 10.2-15.9 25.7c-1.1 9.6-.5 20.9 1.7 28.9c2.2 8.1 5.9 13.3 11.1 15.7c3.2 1.5 9.2 1.4 12.6-.2m99.7 26.3c-5.5-1.2-9.4-2.6-14.1-5.3c-13.4-7.7-22.9-22.6-25.9-40.6c-1.2-7.4-1.2-21.1 0-28.4c2.8-17.3 12.3-32.9 24.9-41c6.6-4.3 17.1-7.3 25.2-7.3c4.7 0 12.4 1.5 17 3.3c11.1 4.3 20.9 14.6 26.5 27.8c3.7 8.8 5.3 17.1 5.4 28.8c.1 7.9 0 8.8-1 9.9c-1.1 1.2-1.2 1.2-33.9 1.2h-32.9v1.9c0 3.4 1.6 10.2 3.3 14.4c2.1 5.1 4.8 8.4 8.6 10.3c3.9 2 10.4 2.1 14.7.2c3.5-1.5 7.5-5.7 8.7-9.1c.5-1.3 1.2-3.4 1.6-4.6c.7-2 1-2.3 2.8-2.4c2.5-.2 22.6 4.2 24.4 5.3c1.8 1.2 1.4 3.7-1.7 10.3s-8.2 13-13.4 16.9c-8.7 6.5-15.8 8.8-28.1 9.1c-5.8 0-9.7-.2-12.1-.7m27.2-72.3c.6-.4.6-1.4-.2-5.4c-1.8-9.8-5.7-16.5-11.1-19c-8.3-3.9-17.6 1.1-21.6 11.5c-1.9 4.8-3.3 12.1-2.6 12.8c.8.7 34.4.8 35.5.1m84 72.2c-13.9-3.2-26.5-13.7-33.9-28.1c-2.4-4.8-5.3-14.4-6.2-20.4c-3.4-23.8 3.4-46.7 17.7-60.4c7.5-7.1 14.2-10.8 23.2-12.6c31.5-6.3 58.6 19.4 60.3 57.3c.7 15.8-3.1 30.9-10.8 42.5c-6.4 9.7-16.5 17.4-27.3 20.8c-5 1.7-17.6 2.1-23 .9m16.9-25.2c5.1-2.6 9-8.8 11.1-17.6c1-4.4 1.2-6.8 1.2-17.7c0-10.1-.2-13.4-1-16.7c-2.9-12.2-7.7-18.5-15.2-20.1c-9.9-2-17.7 5.3-20.6 19.3c-3.5 17-1.4 37.9 4.7 47.2c1.7 2.6 5.7 5.8 8.3 6.7c2.5.7 9 .1 11.5-1.1M40 352.6c-1.1-1.2-1.6-2.8-5.1-15.4c-1.8-6.7-3.7-13.4-5.6-20.1c-2.8-10-5.6-20.1-8.4-30.1c-3.5-12.8-7.1-25.5-10.7-38.3c-1.4-4.8-2.5-9.2-2.5-9.9c0-2.6 1.5-2.9 15.7-2.9c11.9 0 13.3.1 14.4 1c1 .8 1.9 4.1 5.6 20.8c4.4 20 5.9 26.5 7.8 35.2c2.9 12.6 6.1 28 6.1 28.9c0 1.7.9 0 1.6-3.2s2.2-9.9 9.2-41.9c7.4-33.7 8.6-38.6 9.5-39.6c.8-.8 2.8-.9 14.2-1.1c14.2-.2 15.9.1 16.2 2.5c.2 1.1-1.8 8.3-10.3 38.5c-4 14.5-8.1 29.1-12 43.7c-1.9 7.1-3.4 12.3-7.8 27.5c-.6 2.1-1.5 4.1-1.9 4.6c-.7.7-3.3.9-18 .9c-16 0-17.1-.1-18-1.1m87.1-.1l-1.5-1.2l.1-56.6c.1-53.5.2-56.7 1.1-57.6s2.4-1 13.5-1.2c10.5-.1 12.8 0 14.2.7l1.7.9v56.3c0 40.5-.2 56.7-.6 57.7c-.3.8-1.2 1.5-1.8 1.7c-.7.2-6.6.3-13.3.3c-11.2.2-12 .1-13.4-1m8.8-125.5c-5-1.3-9.9-5.9-11.6-10.8c-1.1-3.4-1.2-8.8-.1-12.5c1-3.4 4.9-8 8.3-9.8c3.5-1.8 12.1-2.1 15.7-.5c4.2 1.8 6.8 4.3 8.7 8c1.5 3.1 1.7 3.9 1.7 8.5c0 5.9-1.1 8.8-4.3 12.2c-3.3 3.4-6.8 4.9-12.1 5.2c-2.5.1-5.3-.1-6.3-.3M3.1 198.1c-.8-.2-1.5-.6-2.1-1.1c-.9-.8-1-5.1-1-75.7C0 80 .2 46 .5 45.7c1.1-1.6 3.1-1.9 12.9-1.9c5.5 0 10.4.2 10.9.3c1.4.5 1.9 1.9 3.4 9.3c.8 3.8 1.6 7 1.8 7.2s1.2-1 2.1-2.8c1.9-3.8 7.8-9.9 11.7-12.1c6.7-3.9 14.2-5.4 21.4-4.2c16.9 2.6 28.7 17.1 34 41.7c1.3 6.1 1.6 26.9.6 34.9c-1.9 14.1-7.3 27.2-14.3 34.8c-7.2 7.7-13.6 10.9-22.9 11.4c-6.6.4-10.9-.4-15.9-2.9c-4.5-2.2-9.7-7.1-12.9-12.1c-1.2-1.8-2.3-3.3-2.5-3.3s-.4 10.9-.4 24.2c0 13.6-.2 24.8-.5 25.6c-.8 2.2-3.2 2.6-14.8 2.6c-6.1 0-11.4-.2-12-.3m51.4-60.3c4.1-1.4 8-5.5 10.1-10.8c2.5-5.9 3.6-11.9 3.9-21.2c.6-16.2-3.1-30.1-9.4-35.4c-2-1.7-7.2-3.7-9.6-3.7c-12.1 0-19.9 13.9-19.9 35.4c0 15.3 3.1 26.3 8.9 32.2c4.3 4.1 10.2 5.5 16 3.5m399.8 26.1c-19.3-3.5-34.4-18.5-39.7-39.4c-7.1-28.3.7-58 19.1-72.9c17.4-14 42.4-14 58.4.1c7 6.1 11 12 14.9 21.4c3.2 7.8 4.5 14.5 4.9 25.3c.2 5 .1 9.7 0 10.4c-.8 3.3.9 3.2-34.7 3.2c-17.9 0-32.7.2-32.9.5c-.6 1 1.4 10.9 3.1 15.3c2.2 5.8 6.4 10.3 10.8 11.6c9.5 2.7 18.4-1.5 21.4-10.2c2.1-5.9 2.4-6.3 4.7-6.5c1.7-.1 14.5 2.4 22.3 4.4c3.2.8 3.5 2.4 1.5 7.6c-2.4 6.1-5.3 10.9-9.4 15.3c-9.1 9.7-18.9 13.8-33.8 14.2c-4.4.1-9.1-.1-10.6-.3m26.4-73.8c0-5.6-3.6-16-6.9-19.9c-2.3-2.7-6.7-4.7-10.4-4.6c-5.9 0-10.4 2.8-13.7 8.4c-2.1 3.7-4.5 10.7-5.1 15l-.4 2.8h36.4zM116 160.5l-1.5-1.2v-56.8c0-52.1.1-56.9.9-57.6c1.2-1 20.8-1.4 23.1-.5c1.9.8 2.5 2.1 3.9 9.8c.6 3.4 1.3 6.3 1.5 6.5s1.3-1.3 2.3-3.3c2.4-4.6 8.1-10.6 12-12.6c6.5-3.3 16-4.2 23.3-2.1c4.6 1.3 6.5 2.9 6.5 5.5c0 1.9-3.7 17.5-5.4 22.6c-1.3 3.9-2.1 4.1-7.6 2c-6.5-2.4-9.8-3.3-12.9-3.3c-8.4 0-13.7 4.4-16.4 13.7c-.9 2.9-1 7.4-1.3 40c-.2 29-.5 36.8-1 37.4c-.5.5-3.9.8-13.4.9c-12 .2-12.6.1-14-1m82.5.6c-.6-.3-1.3-.9-1.5-1.3c-.2-.6-.6-102.4-.4-112.7c.1-2.9 1.8-3.2 15.8-3.2c13.8 0 14.1.1 14.4 3.6c.1.9.1 26.5.1 56.8c-.1 54.3-.1 55.2-1.2 56.3s-1.8 1.1-13.5 1.1c-7-.1-13.1-.4-13.7-.6m49-.1c-.7-.3-1.5-1.2-1.8-2c-.3-.9-.5-21.9-.5-57.1c0-47.2.1-55.9.8-56.8c.7-1 1.7-1.1 11.5-1.3c7.2-.1 11.2 0 12.2.5c1.7.7 1.9 1.3 3.9 11.3c.5 2.7 1.1 5 1.3 5.2s1.2-1.4 2.3-3.5c3.5-7 9.3-12.2 16.3-14.7c2.6-1 4.6-1.2 9.7-1.3c6.8-.1 9.6.6 15.3 3.5c3.8 1.9 9.7 7.9 12.2 12.5c1.2 2.1 2.3 3.8 2.5 3.8s1.2-1.2 2.2-2.7c5.2-7.6 12.7-13.6 19.9-15.9c5.7-1.8 14.7-1.8 20 .1c11.4 3.9 18.2 12.4 22.4 28.2c1.2 4.4 1.2 5.7 1.4 46.4c.2 36.1.1 42-.6 43c-.8 1.1-1.6 1.1-13.6 1.3c-12.2.2-12.8.1-14.3-1l-1.5-1.2l-.2-37.8c-.2-37.3-.2-37.9-1.4-41.5c-2.5-7.5-6.6-10.7-13.5-10.8c-7-.1-12 3.7-14.6 11.1c-1.9 5.3-2.2 12-2.2 45.9c0 32.3 0 33.1-1.1 34.2c-1 1-1.9 1.1-13.4 1.1c-12.6 0-14-.2-15-2.2c-.4-.7-.6-12.7-.6-36.1c0-35.1-.2-39.3-2-45c-2.6-8.1-12.5-11.5-20.4-7.2c-5.1 2.8-7.8 8.5-8.8 18.6c-.3 2.9-.5 19.8-.5 37.5v32.2l-1.2 1.1c-1.2 1.1-1.9 1.1-13.3 1.1c-6.7.1-12.7-.2-13.4-.5M206.6 34.9c-4.9-1.5-9.2-5.3-11.3-9.8c-1.8-3.9-1.8-11.1 0-15.1c1.6-3.6 5.1-7.1 8.7-8.7c4-1.8 11.1-1.7 15.2.1c4 1.7 8 6.2 9.3 10.4c1.3 4.3.9 10.2-1 14.4c-1.7 3.7-5.9 7.2-10 8.4c-2.9.9-8.5 1-10.9.3)"},
        }},
        {"tiktok", {0, 0, 24, 24}, {
            {{}, R"(M12.525.02c1.31-.02 2.61-.01 3.91-.02.08 1.53.63 3.09 1.75 4.17 1.12 1.11 2.7 1.62 4.24 1.79v4.03c-1.44-.05-2.89-.35-4.2-.97-.57-.26-1.1-.59-1.62-.93-.01 2.92.01 5.84-.02 8.75-.08 1.4-.54 2.79-1.35 3.94-1.31 1.92-3.58 3.17-5.91 3.21-1.43.08-2.86-.31-4.08-1.03-2.02-1.19-3.44-3.37-3.65-5.71-.02-.5-.03-1-.01-1.49.18-1.9 1.12-3.72 2.58-4.96 1.66-1.44 3.98-2.13 6.15-1.72.02 1.48-.04 2.96-.04 4.44-.99-.32-2.15-.23-3.02.37-.63.41-1.11 1.04-1.36 1.75-.21.51-.15 1.07-.14 1.61.24 1.64 1.82 3.02 3.5 2.87 1.12-.01 2.19-.66 2.77-1.61.19-.33.4-.67.41-1.06.1-1.79.06-3.57.07-5.36.01-4.03-.01-8.05.02-12.07z)"},
        }},
        {"google_search", {0, 0, 24, 24}, {
            {{0xff4285f4}, R"(M12.48 10.92v3.28h7.84c-.24 1.84-.853 3.187-1.787 4.133-1.147 1.147-2.933 2.4-6.053 2.4-4.827 0-8.6-3.893-8.6-8.72s3.773-8.72 8.6-8.72c2.6 0 4.507 1.027 5.907 2.347l2.307-2.307C18.747 1.44 16.133 0 12.48 0 5.867 0 .307 5.387.307 12s5.56 12 12.173 12c3.573 0 6.267-1.173 8.373-3.36 2.16-2.16 2.84-5.213 2.84-7.667 0-.76-.053-1.467-.173-2.053H12.48z)"},
        }},
        {"wikipedia", {0, 0, 24, 24}, {
            {{}, R"(M12.09 13.119c-.936 1.932-2.217 4.548-2.853 5.728-.616 1.074-1.127.931-1.532.029-1.406-3.321-4.293-9.144-5.651-12.409-.251-.601-.441-.987-.619-1.139-.181-.15-.554-.24-1.122-.271C.103 5.033 0 4.982 0 4.898v-.455l.052-.045c.924-.005 5.401 0 5.401 0l.051.045v.434c0 .119-.075.176-.225.176l-.564.031c-.485.029-.727.164-.727.436 0 .135.053.33.166.601 1.082 2.646 4.818 10.521 4.818 10.521l.136.046 2.411-4.81-.482-1.067-1.658-3.264s-.318-.654-.428-.872c-.728-1.443-.712-1.518-1.447-1.617-.207-.023-.313-.05-.313-.149v-.468l.06-.045h4.292l.113.037v.451c0 .105-.076.15-.227.15l-.308.047c-.792.061-.661.381-.136 1.422l1.582 3.252 1.758-3.504c.293-.64.233-.801.111-.947-.07-.084-.305-.22-.812-.24l-.201-.021c-.052 0-.098-.015-.145-.051-.045-.031-.067-.076-.067-.129v-.427l.061-.045c1.247-.008 4.043 0 4.043 0l.059.045v.436c0 .121-.059.178-.193.178-.646.03-.782.095-1.023.439-.12.186-.375.589-.646 1.039l-2.301 4.273-.065.135 2.792 5.712.17.048 4.396-10.438c.154-.422.129-.722-.064-.895-.197-.172-.346-.273-.857-.295l-.42-.016c-.061 0-.105-.014-.152-.045-.043-.029-.072-.075-.072-.119v-.436l.059-.045h4.961l.041.045v.437c0 .119-.074.18-.209.18-.648.03-1.127.18-1.443.421-.314.255-.557.616-.736 1.067 0 0-4.043 9.258-5.426 12.339-.525 1.007-1.053.917-1.503-.031-.571-1.171-1.773-3.786-2.646-5.71l.053-.036z)"},
        }},
        {"steam", {0, 0, 24, 24}, {
            {{}, R"(M11.979 0C5.678 0 .511 4.86.022 11.037l6.432 2.658c.545-.371 1.203-.59 1.912-.59.063 0 .125.004.188.006l2.861-4.142V8.91c0-2.495 2.028-4.524 4.524-4.524 2.494 0 4.524 2.031 4.524 4.527s-2.03 4.525-4.524 4.525h-.105l-4.076 2.911c0 .052.004.105.004.159 0 1.875-1.515 3.396-3.39 3.396-1.635 0-3.016-1.173-3.331-2.727L.436 15.27C1.862 20.307 6.486 24 11.979 24c6.627 0 11.999-5.373 11.999-12S18.605 0 11.979 0zM7.54 18.21l-1.473-.61c.262.543.714.999 1.314 1.25 1.297.539 2.793-.076 3.332-1.375.263-.63.264-1.319.005-1.949s-.75-1.121-1.377-1.383c-.624-.26-1.29-.249-1.878-.03l1.523.63c.956.4 1.409 1.5 1.009 2.455-.397.957-1.497 1.41-2.454 1.012H7.54zm11.415-9.303c0-1.662-1.353-3.015-3.015-3.015-1.665 0-3.015 1.353-3.015 3.015 0 1.665 1.35 3.015 3.015 3.015 1.663 0 3.015-1.35 3.015-3.015zm-5.273-.005c0-1.252 1.013-2.266 2.265-2.266 1.249 0 2.266 1.014 2.266 2.266 0 1.251-1.017 2.265-2.266 2.265-1.253 0-2.265-1.014-2.265-2.265z)"},
        }},
    };

    constexpr QRgb kServiceIconTileColors[] = {0xff5b7fd6, 0xff2e9c8f, 0xff4c9a4f, 0xffc9892b, 0xffd9674e, 0xffc9578a, 0xff8a63c9, 0xff3794b8};
    constexpr int kServiceIconStripSize = 16;

    const ServiceIconGlyph *serviceIconGlyph(const QString &id) {
        for (const auto &glyph : kServiceIconGlyphs) {
            if (id == QLatin1String(glyph.id)) return &glyph;
        }
        return nullptr;
    }

    bool serviceIconIsDigit(char c) {
        return c >= '0' && c <= '9';
    }

    struct ServiceIconPathReader {
        const char *p;
        const char *end;

        void skipSeparators() {
            while (p < end && (*p == ' ' || *p == ',' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\f')) ++p;
        }

        bool atNumber() {
            skipSeparators();
            return p < end && (serviceIconIsDigit(*p) || *p == '.' || *p == '-' || *p == '+');
        }

        // Stops where the next number starts: "1.5.5" is 1.5 then .5, "5-3" is 5 then -3.
        bool number(qreal &value) {
            skipSeparators();
            const char *q = p;
            const bool negative = q < end && *q == '-';
            if (q < end && (*q == '-' || *q == '+')) ++q;
            qreal mantissa = 0;
            int exponent = 0;
            bool digits = false;
            while (q < end && serviceIconIsDigit(*q)) {
                mantissa = mantissa * 10 + (*q++ - '0');
                digits = true;
            }
            if (q < end && *q == '.') {
                ++q;
                while (q < end && serviceIconIsDigit(*q)) {
                    mantissa = mantissa * 10 + (*q++ - '0');
                    --exponent;
                    digits = true;
                }
            }
            if (!digits) return false;
            if (q < end && (*q == 'e' || *q == 'E')) {
                const char *e = q + 1;
                const bool negativeExponent = e < end && *e == '-';
                if (e < end && (*e == '-' || *e == '+')) ++e;
                if (e < end && serviceIconIsDigit(*e)) {
                    int power = 0;
                    while (e < end && serviceIconIsDigit(*e)) power = qMin(power * 10 + (*e++ - '0'), 9999);
                    exponent += negativeExponent ? -power : power;
                    q = e;
                }
            }
            value = exponent < 0 ? mantissa / qPow(10.0, -exponent) : mantissa * qPow(10.0, exponent);
            if (negative) value = -value;
            p = q;
            return qIsFinite(value);
        }

        // Arc flags are single characters, so "01.5" is two flags and a number.
        bool flag(bool &value) {
            skipSeparators();
            if (p == end || (*p != '0' && *p != '1')) return false;
            value = *p++ == '1';
            return true;
        }

        bool point(QPointF &value) {
            qreal x = 0;
            qreal y = 0;
            if (!number(x) || !number(y)) return false;
            value = QPointF(x, y);
            return true;
        }
    };

    // SVG implementation notes F.6.5 and F.6.6: center parameterization with radii scaled up to fit, then cubics of at most 90 degrees.
    void serviceIconArcTo(QPainterPath &path, const QPointF &from, qreal rx, qreal ry, qreal rotation, bool largeArc, bool sweep, const QPointF &to) {
        if (from == to) return;
        rx = qAbs(rx);
        ry = qAbs(ry);
        if (rx == 0 || ry == 0) {
            path.lineTo(to);
            return;
        }
        const qreal cosPhi = qCos(qDegreesToRadians(rotation));
        const qreal sinPhi = qSin(qDegreesToRadians(rotation));
        const qreal hx = (from.x() - to.x()) / 2;
        const qreal hy = (from.y() - to.y()) / 2;
        const qreal x1 = cosPhi * hx + sinPhi * hy;
        const qreal y1 = cosPhi * hy - sinPhi * hx;
        const qreal lambda = (x1 * x1) / (rx * rx) + (y1 * y1) / (ry * ry);
        if (lambda > 1) {
            rx *= qSqrt(lambda);
            ry *= qSqrt(lambda);
        }
        const qreal denominator = rx * rx * y1 * y1 + ry * ry * x1 * x1;
        qreal factor = denominator > 0 ? qSqrt(qMax<qreal>(0, (rx * rx * ry * ry - denominator) / denominator)) : 0;
        if (largeArc == sweep) factor = -factor;
        const qreal cx1 = factor * rx * y1 / ry;
        const qreal cy1 = -factor * ry * x1 / rx;
        const qreal cx = cosPhi * cx1 - sinPhi * cy1 + (from.x() + to.x()) / 2;
        const qreal cy = sinPhi * cx1 + cosPhi * cy1 + (from.y() + to.y()) / 2;
        const qreal ux = (x1 - cx1) / rx;
        const qreal uy = (y1 - cy1) / ry;
        const qreal vx = (-x1 - cx1) / rx;
        const qreal vy = (-y1 - cy1) / ry;
        const qreal theta = qAtan2(uy, ux);
        qreal delta = qAtan2(ux * vy - uy * vx, ux * vx + uy * vy);
        if (!sweep && delta > 0) delta -= 2 * M_PI;
        if (sweep && delta < 0) delta += 2 * M_PI;
        const int segments = qMax(1, qCeil(qAbs(delta) / M_PI_2 - 1e-6));
        const qreal step = delta / segments;
        const qreal k = 4.0 / 3.0 * qTan(step / 4);
        const auto onEllipse = [&](qreal x, qreal y) {
            return QPointF(cx + rx * x * cosPhi - ry * y * sinPhi, cy + rx * x * sinPhi + ry * y * cosPhi);
        };
        for (int i = 0; i < segments; ++i) {
            const qreal a = theta + i * step;
            const qreal b = a + step;
            path.cubicTo(onEllipse(qCos(a) - k * qSin(a), qSin(a) + k * qCos(a)),
                         onEllipse(qCos(b) + k * qSin(b), qSin(b) - k * qCos(b)),
                         i + 1 == segments ? to : onEllipse(qCos(b), qSin(b)));
        }
    }

    // Malformed data ends the path at the last complete command, as SVG renderers do.
    QPainterPath serviceIconParsePath(const char *data) {
        ServiceIconPathReader reader{data, data + qstrlen(data)};
        QPainterPath path;
        path.setFillRule(Qt::WindingFill);
        QPointF current;
        QPointF start;
        QPointF control;
        char command = 0;
        char previous = 0;
        while (true) {
            reader.skipSeparators();
            if (reader.p == reader.end) break;
            const char c = *reader.p;
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
                command = c;
                ++reader.p;
            } else if (command == 0 || command == 'Z' || command == 'z' || !reader.atNumber()) {
                break;
            }
            const char kind = char(command | 0x20);
            if (previous == 0 && kind != 'm') break;
            const bool relative = command == kind;
            const QPointF origin = relative ? current : QPointF();
            QPointF c1;
            QPointF c2;
            QPointF to;
            qreal n = 0;
            qreal rx = 0;
            qreal ry = 0;
            bool largeArc = false;
            bool sweep = false;
            switch (kind) {
                case 'm':
                    if (!reader.point(to)) return path;
                    to += origin;
                    path.moveTo(to);
                    start = to;
                    command = relative ? 'l' : 'L';
                    break;
                case 'l':
                    if (!reader.point(to)) return path;
                    to += origin;
                    path.lineTo(to);
                    break;
                case 'h':
                    if (!reader.number(n)) return path;
                    to = QPointF(origin.x() + n, current.y());
                    path.lineTo(to);
                    break;
                case 'v':
                    if (!reader.number(n)) return path;
                    to = QPointF(current.x(), origin.y() + n);
                    path.lineTo(to);
                    break;
                case 'c':
                    if (!reader.point(c1) || !reader.point(c2) || !reader.point(to)) return path;
                    control = c2 + origin;
                    to += origin;
                    path.cubicTo(c1 + origin, control, to);
                    break;
                case 's':
                    if (!reader.point(c2) || !reader.point(to)) return path;
                    c1 = previous == 'c' || previous == 's' ? 2 * current - control : current;
                    control = c2 + origin;
                    to += origin;
                    path.cubicTo(c1, control, to);
                    break;
                case 'q':
                    if (!reader.point(c1) || !reader.point(to)) return path;
                    control = c1 + origin;
                    to += origin;
                    path.quadTo(control, to);
                    break;
                case 't':
                    if (!reader.point(to)) return path;
                    control = previous == 'q' || previous == 't' ? 2 * current - control : current;
                    to += origin;
                    path.quadTo(control, to);
                    break;
                case 'a':
                    if (!reader.number(rx) || !reader.number(ry) || !reader.number(n) || !reader.flag(largeArc) || !reader.flag(sweep) || !reader.point(to)) return path;
                    to += origin;
                    serviceIconArcTo(path, current, rx, ry, n, largeArc, sweep, to);
                    break;
                case 'z':
                    path.closeSubpath();
                    to = start;
                    break;
                default:
                    return path;
            }
            previous = kind;
            current = to;
        }
        return path;
    }

    QPainterPath serviceIconLayerPath(const char *data) {
        static QHash<const char *, QPainterPath> parsed;
        auto it = parsed.constFind(data);
        if (it == parsed.cend()) it = parsed.insert(data, serviceIconParsePath(data));
        return *it;
    }

    QTransform serviceIconFit(const QRectF &source, const QRectF &target) {
        const qreal scale = qMin(target.width() / source.width(), target.height() / source.height());
        QTransform transform;
        transform.translate(target.center().x(), target.center().y());
        transform.scale(scale, scale);
        transform.translate(-source.center().x(), -source.center().y());
        return transform;
    }

    QBrush serviceIconLayerBrush(const ServiceIconLayer &layer, const QRectF &bounds, const QColor &foreground) {
        QList<QColor> stops;
        for (const QRgb rgb : layer.fill) {
            if (rgb != 0) stops << QColor::fromRgb(rgb);
        }
        if (stops.isEmpty()) return foreground;
        if (stops.size() == 1) return stops.first();
        QLinearGradient gradient(bounds.topLeft(), bounds.bottomLeft());
        for (qsizetype i = 0; i < stops.size(); ++i) gradient.setColorAt(qreal(i) / qreal(stops.size() - 1), stops[i]);
        return gradient;
    }

    QString serviceIconInitials(const QString &label) {
        QList<QStringList> words;
        for (const QString &word : label.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
            QStringList letters;
            for (qsizetype i = 0; i < word.size();) {
                const qsizetype length = word[i].isHighSurrogate() && i + 1 < word.size() && word[i + 1].isLowSurrogate() ? 2 : 1;
                const char32_t ucs = length == 2 ? QChar::surrogateToUcs4(word[i], word[i + 1]) : char32_t(word[i].unicode());
                if (QChar::isLetterOrNumber(ucs)) letters << word.mid(i, length);
                i += length;
            }
            if (!letters.isEmpty()) words << letters;
        }
        if (words.isEmpty()) return {};
        const QString initials = words.size() > 1 ? words[0][0] + words[1][0] : words[0].mid(0, 2).join(QString());
        return initials.toUpper();
    }

    QColor serviceIconTileColor(const QString &label) {
        quint32 hash = 2166136261u;
        for (const QChar c : label) hash = (hash ^ c.unicode()) * 16777619u;
        return QColor::fromRgb(kServiceIconTileColors[hash % std::size(kServiceIconTileColors)]);
    }

    void serviceIconPaintTile(QPainter &painter, const QRectF &area, const QString &label, const QColor &color) {
        const qreal radius = area.width() * 5 / 24;
        painter.setBrush(color);
        painter.drawRoundedRect(area, radius, radius);
        const QString initials = serviceIconInitials(label);
        if (initials.isEmpty()) return;
        QFont font;
        font.setPixelSize(48);
        font.setWeight(QFont::Medium);
        QPainterPath text;
        text.addText(0, 0, font, initials);
        if (text.boundingRect().isEmpty()) return;
        QRectF box(0, 0, area.width() * 0.72, area.height() * 0.42);
        box.moveCenter(area.center());
        painter.fillPath(serviceIconFit(text.boundingRect(), box).map(text), Qt::white);
    }

    void serviceIconPaintBadge(QPainter &painter, ServiceIcons::State state, const QColor &color) {
        const QPointF center(19, 19);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::black);
        painter.setCompositionMode(QPainter::CompositionMode_Clear);
        painter.drawEllipse(center, 5.7, 5.7);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.setBrush(color);
        painter.drawEllipse(center, 4.6, 4.6);
        painter.setPen(QPen(Qt::white, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        if (state == ServiceIcons::State::Access) {
            painter.drawPolyline(QPolygonF{{16.6, 19.2}, {18.2, 20.8}, {21.2, 17.6}});
        } else if (state == ServiceIcons::State::Partial) {
            painter.drawLine(QPointF(16.6, 19), QPointF(21.4, 19));
        } else {
            painter.drawLine(QPointF(17.2, 17.2), QPointF(20.8, 20.8));
            painter.drawLine(QPointF(20.8, 17.2), QPointF(17.2, 20.8));
        }
    }

    bool serviceIconSameItems(const QList<ServiceIcons::Item> &a, const QList<ServiceIcons::Item> &b) {
        if (a.size() != b.size()) return false;
        for (qsizetype i = 0; i < a.size(); ++i) {
            if (a[i].id != b[i].id || a[i].name != b[i].name || a[i].state != b[i].state || a[i].toolTip != b[i].toolTip) return false;
        }
        return true;
    }
} // namespace

namespace ServiceIcons {
    State StateOf(const QString &status) {
        if (status == QLatin1String("available")) return State::Access;
        if (status == QLatin1String("partial")) return State::Partial;
        if (status == QLatin1String("restricted") || status == QLatin1String("blocked")) return State::NoAccess;
        return State::Unknown;
    }

    // GUI thread only: the parsed paths and QPixmapCache are unsynchronized.
    QPixmap Pixmap(const QString &id, const QString &name, State state, int size, qreal devicePixelRatio, const QColor &foreground) {
        if (size <= 0) return {};
        if (!(devicePixelRatio > 0)) devicePixelRatio = 1;
        const ThemeTokens &tokens = themeManager()->tokens;
        const ServiceIconGlyph *glyph = serviceIconGlyph(id);
        const QString key = QStringLiteral("service-icon/%1/%2/%3/%4/%5/%6/%7/%8/%9/")
                                .arg(static_cast<int>(state))
                                .arg(size)
                                .arg(devicePixelRatio)
                                .arg(foreground.rgba())
                                .arg(tokens.success.rgba())
                                .arg(tokens.warning.rgba())
                                .arg(tokens.danger.rgba())
                                .arg(tokens.muted.rgba())
                                .arg(id.size())
                            + id + (glyph ? QString() : name);
        QPixmap pixmap;
        if (QPixmapCache::find(key, &pixmap)) return pixmap;

        const int pixels = qRound(size * devicePixelRatio);
        pixmap = QPixmap(pixels, pixels);
        pixmap.setDevicePixelRatio(devicePixelRatio);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.scale(size / 24.0, size / 24.0);
        const bool muted = state == State::NoAccess || state == State::Unknown;
        const QRectF area = state == State::Plain ? QRectF(0, 0, 24, 24) : QRectF(0, 0, 20.2, 20.2);
        if (glyph != nullptr) {
            const QTransform fit = serviceIconFit(glyph->viewBox, area);
            for (const auto &layer : glyph->layers) {
                if (layer.path == nullptr) break;
                const QPainterPath path = fit.map(serviceIconLayerPath(layer.path));
                painter.fillPath(path, muted ? QBrush(tokens.muted) : serviceIconLayerBrush(layer, path.boundingRect(), foreground));
            }
        } else {
            const QString label = name.trimmed().isEmpty() ? id : name;
            serviceIconPaintTile(painter, area, label, muted ? tokens.muted : serviceIconTileColor(label));
        }
        if (state == State::Access) serviceIconPaintBadge(painter, state, tokens.success);
        else if (state == State::Partial) serviceIconPaintBadge(painter, state, tokens.warning);
        else if (state == State::NoAccess) serviceIconPaintBadge(painter, state, tokens.danger);
        painter.end();
        QPixmapCache::insert(key, pixmap);
        return pixmap;
    }

    int StripWidth(qsizetype count, int size) {
        return count > 0 ? static_cast<int>(count) * size + static_cast<int>(count - 1) * kSpacing : 0;
    }

    QList<QRect> StripRects(const QRect &area, qsizetype count, int size) {
        QList<QRect> rects;
        rects.reserve(count);
        const int y = area.top() + (area.height() - size) / 2;
        for (qsizetype i = 0; i < count; ++i) rects << QRect(area.left() + static_cast<int>(i) * (size + kSpacing), y, size, size);
        return rects;
    }

    QList<QRect> PaintStrip(QPainter *painter, const QRect &area, const QList<Item> &items, int size, const QColor &foreground) {
        if (painter->device() == nullptr) return {};
        const qreal devicePixelRatio = painter->device()->devicePixelRatioF();
        const auto rects = StripRects(area, items.size(), size);
        for (qsizetype i = 0; i < items.size(); ++i) {
            painter->drawPixmap(rects[i].topLeft(), Pixmap(items[i].id, items[i].name, items[i].state, size, devicePixelRatio, foreground));
        }
        return rects;
    }
} // namespace ServiceIcons

ServiceIconStrip::ServiceIconStrip(QWidget *parent) : QWidget(parent) {}

void ServiceIconStrip::setItems(const QList<ServiceIcons::Item> &items) {
    if (serviceIconSameItems(items, items_)) return;
    items_ = items;
    updateGeometry();
    update();
}

QSize ServiceIconStrip::sizeHint() const {
    return {ServiceIcons::StripWidth(items_.size(), kServiceIconStripSize), kServiceIconStripSize};
}

QSize ServiceIconStrip::minimumSizeHint() const {
    return sizeHint();
}

void ServiceIconStrip::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    rects_ = ServiceIcons::PaintStrip(&painter, rect(), items_, kServiceIconStripSize, palette().color(QPalette::WindowText));
}

bool ServiceIconStrip::event(QEvent *event) {
    if (event->type() != QEvent::ToolTip) return QWidget::event(event);
    const auto *help = static_cast<QHelpEvent *>(event);
    for (qsizetype i = 0; i < rects_.size() && i < items_.size(); ++i) {
        if (rects_[i].contains(help->pos())) {
            QToolTip::showText(help->globalPos(), items_[i].toolTip, this, rects_[i]);
            return true;
        }
    }
    QToolTip::hideText();
    event->ignore();
    return true;
}
