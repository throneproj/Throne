#pragma once

#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QVector>

// Fixed-capacity columnar ring: one timestamp per point plus one float column per key, NaN where a key has no value.
class SeriesBuffer {
public:
    struct Marker {
        qint64 t = 0;
        int kind = 0;
        QString text;
    };

    explicit SeriesBuffer(int capacity = 600);

    int capacity() const { return m_capacity; }
    int size() const { return m_size; }
    bool isEmpty() const { return m_size == 0; }

    int addColumn(const QString &key);
    int findColumn(const QString &key) const;
    // Columns outside keep whose values were all evicted lose their key; addColumn() reuses their index.
    void releaseDeadColumns(const QSet<QString> &keep);

    void push(qint64 t, bool running);
    void setLatest(int column, float value);

    // Logical index 0 is the oldest point.
    qint64 time(int i) const { return m_times[slot(i)]; }
    bool running(int i) const { return m_running[slot(i)]; }
    float value(int column, int i) const { return m_columns[column].values[slot(i)]; }
    qint64 latestTime() const { return time(m_size - 1); }
    int lowerBound(qint64 t) const;

    void addMarker(qint64 t, int kind, const QString &text);
    const QList<Marker> &markers() const { return m_markers; }

    void clear();

private:
    struct Column {
        QString key;
        QVector<float> values;
        qint64 lastSerial = -1;
    };

    int slot(int i) const {
        const int s = m_head + i;
        return s >= m_capacity ? s - m_capacity : s;
    }

    int m_capacity;
    int m_head = 0;
    int m_size = 0;
    qint64 m_serial = 0;
    QVector<qint64> m_times;
    QVector<bool> m_running;
    QVector<Column> m_columns;
    QHash<QString, int> m_index;
    QList<Marker> m_markers;
};
