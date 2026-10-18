#include "include/ui/widget/SeriesBuffer.h"

#include <algorithm>
#include <cmath>
#include <limits>

SeriesBuffer::SeriesBuffer(int capacity) : m_capacity(std::max(1, capacity)) {
    m_times.resize(m_capacity);
    m_running.resize(m_capacity);
}

int SeriesBuffer::addColumn(const QString &key) {
    if (const auto it = m_index.constFind(key); it != m_index.constEnd()) return it.value();
    int index = -1;
    for (int i = 0; i < m_columns.size(); ++i) {
        if (m_columns[i].key.isEmpty()) {
            index = i;
            break;
        }
    }
    if (index < 0) {
        index = m_columns.size();
        m_columns.append(Column{});
        m_columns[index].values.resize(m_capacity);
    }
    Column &column = m_columns[index];
    column.key = key;
    column.values.fill(std::numeric_limits<float>::quiet_NaN());
    column.lastSerial = m_serial - 1;
    m_index.insert(key, index);
    return index;
}

int SeriesBuffer::findColumn(const QString &key) const {
    return m_index.value(key, -1);
}

void SeriesBuffer::releaseDeadColumns(const QSet<QString> &keep) {
    const qint64 oldestSerial = m_serial - m_size;
    for (auto &column : m_columns) {
        if (column.key.isEmpty() || keep.contains(column.key) || column.lastSerial >= oldestSerial) continue;
        m_index.remove(column.key);
        column.key.clear();
    }
}

void SeriesBuffer::push(qint64 t, bool running) {
    int s;
    const bool evicting = m_size == m_capacity;
    if (evicting) {
        s = m_head;
        m_head = m_head + 1 == m_capacity ? 0 : m_head + 1;
    } else {
        s = slot(m_size);
        ++m_size;
    }
    m_times[s] = t;
    m_running[s] = running;
    for (auto &column : m_columns) column.values[s] = std::numeric_limits<float>::quiet_NaN();
    ++m_serial;
    if (!evicting) return;
    const qint64 oldest = time(0);
    while (!m_markers.isEmpty() && m_markers.first().t < oldest) m_markers.removeFirst();
}

void SeriesBuffer::setLatest(int column, float value) {
    if (m_size == 0 || column < 0 || column >= m_columns.size()) return;
    m_columns[column].values[slot(m_size - 1)] = value;
    if (!std::isnan(value)) m_columns[column].lastSerial = m_serial - 1;
}

int SeriesBuffer::lowerBound(qint64 t) const {
    int lo = 0, hi = m_size;
    while (lo < hi) {
        const int mid = (lo + hi) / 2;
        if (time(mid) < t) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

void SeriesBuffer::addMarker(qint64 t, int kind, const QString &text) {
    m_markers.append(Marker{t, kind, text});
}

void SeriesBuffer::clear() {
    m_head = 0;
    m_size = 0;
    m_markers.clear();
}
