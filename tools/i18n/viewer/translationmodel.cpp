#include "translationmodel.h"
#include <QColor>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

TranslationModel::TranslationModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

TranslationModel::~TranslationModel()
{
    clearRows();
}

bool TranslationModel::openDatabase(const QString &path, QString *error)
{
    m_dbPath = path;
    return reload(error);
}

bool TranslationModel::reload(QString *error)
{
    clearRows();

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "i18n_viewer");
    db.setDatabaseName(m_dbPath);

    if (!db.open()) {
        if (error) *error = db.lastError().text();
        return false;
    }

    // Load languages
    {
        QSqlQuery q(db);
        if (!q.exec("SELECT lang, plural_rule FROM i18n_language ORDER BY lang")) {
            if (error) *error = q.lastError().text();
            return false;
        }
        while (q.next()) {
            QString lang = q.value(0).toString();
            m_languages.append(lang);
            m_pluralRules[lang] = q.value(1).toString();
        }
    }

    // Load messages
    {
        QSqlQuery q(db);
        if (!q.exec("SELECT lang, ctx, msgid, form, msgstr FROM i18n_message "
                    "ORDER BY lang, ctx, msgid, form")) {
            if (error) *error = q.lastError().text();
            return false;
        }
        while (q.next()) {
            TranslationRow row;
            row.lang = q.value(0).toString();
            row.ctx = q.value(1).toString();
            row.msgid = q.value(2).toString();
            row.form = q.value(3).toInt();
            row.msgstr = q.value(4).toString();
            row.pluralRule = m_pluralRules.value(row.lang);
            m_rows.append(row);
        }
    }

    db.close();
    QSqlDatabase::removeDatabase("i18n_viewer");

    m_original = m_rows;
    rebuildVisible();
    return true;
}

void TranslationModel::clearRows()
{
    beginResetModel();
    m_rows.clear();
    m_original.clear();
    m_visible.clear();
    m_languages.clear();
    m_pluralRules.clear();
    endResetModel();
}

void TranslationModel::rebuildVisible()
{
    m_visible.clear();
    m_visible.reserve(m_rows.size());

    for (int i = 0; i < m_rows.size(); ++i) {
        const TranslationRow &row = m_rows[i];

        // Language filter
        if (!m_languageFilter.isEmpty() && row.lang != m_languageFilter)
            continue;

        // Empty filter
        if (m_onlyEmpty && !row.msgstr.isEmpty())
            continue;

        // Text filter
        if (!m_textFilter.isEmpty()) {
            QString needle = m_textFilter.toLower();
            bool match = row.ctx.toLower().contains(needle) ||
                         row.msgid.toLower().contains(needle) ||
                         row.msgstr.toLower().contains(needle);
            if (!match)
                continue;
        }

        m_visible.append(i);
    }
}

void TranslationModel::setLanguageFilter(const QString &lang)
{
    if (m_languageFilter == lang)
        return;
    m_languageFilter = lang;
    beginResetModel();
    rebuildVisible();
    endResetModel();
}

void TranslationModel::setTextFilter(const QString &needle)
{
    if (m_textFilter == needle)
        return;
    m_textFilter = needle;
    beginResetModel();
    rebuildVisible();
    endResetModel();
}

void TranslationModel::setOnlyEmpty(bool onlyEmpty)
{
    if (m_onlyEmpty == onlyEmpty)
        return;
    m_onlyEmpty = onlyEmpty;
    beginResetModel();
    rebuildVisible();
    endResetModel();
}

int TranslationModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_visible.size();
}

int TranslationModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return ColumnCount;
}

QVariant TranslationModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_visible.size())
        return QVariant();

    int row = m_visible[index.row()];
    const TranslationRow &r = m_rows[row];

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
        case ColLang:    return r.lang;
        case ColCtx:     return r.ctx;
        case ColMsgid:   return r.msgid;
        case ColForm:    return r.form;
        case ColMsgstr:  return r.msgstr;
        }
    }

    if (role == Qt::ForegroundRole) {
        if (index.column() == ColMsgstr && r.msgstr.isEmpty())
            return QColor(180, 0, 0);  // Red for untranslated
    }

    if (role == Qt::ToolTipRole) {
        if (!r.pluralRule.isEmpty())
            return QString("Plural rule: %1").arg(r.pluralRule);
    }

    return QVariant();
}

QVariant TranslationModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    switch (section) {
    case ColLang:    return "Language";
    case ColCtx:     return "Context";
    case ColMsgid:   return "Source";
    case ColForm:    return "Form";
    case ColMsgstr:  return "Translation";
    }
    return QVariant();
}

Qt::ItemFlags TranslationModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;

    // Only msgstr is editable
    if (index.column() == ColMsgstr)
        f |= Qt::ItemIsEditable;

    return f;
}

bool TranslationModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.column() != ColMsgstr || role != Qt::EditRole)
        return false;

    setMsgstr(index.row(), value.toString());
    return true;
}

void TranslationModel::setMsgstr(int viewRow, const QString &text)
{
    if (viewRow < 0 || viewRow >= m_visible.size())
        return;

    int row = m_visible[viewRow];
    m_rows[row].msgstr = text;

    QModelIndex idx = index(viewRow, ColMsgstr);
    emit dataChanged(idx, idx, {Qt::DisplayRole, Qt::EditRole, Qt::ForegroundRole});
}

int TranslationModel::dirtyCount() const
{
    int count = 0;
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].msgstr != m_original[i].msgstr)
            ++count;
    }
    return count;
}

bool TranslationModel::submitChanges(QString *error)
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "i18n_viewer_submit");
    db.setDatabaseName(m_dbPath);

    if (!db.open()) {
        if (error) *error = db.lastError().text();
        return false;
    }

    db.transaction();

    QSqlQuery q(db);
    q.prepare("UPDATE i18n_message SET msgstr = ? "
              "WHERE lang = ? AND ctx = ? AND msgid = ? AND form = ?");

    int updated = 0;
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].msgstr == m_original[i].msgstr)
            continue;

        const TranslationRow &r = m_rows[i];
        q.bindValue(0, r.msgstr);
        q.bindValue(1, r.lang);
        q.bindValue(2, r.ctx);
        q.bindValue(3, r.msgid);
        q.bindValue(4, r.form);

        if (!q.exec()) {
            db.rollback();
            db.close();
            QSqlDatabase::removeDatabase("i18n_viewer_submit");
            if (error) *error = q.lastError().text();
            return false;
        }
        ++updated;
    }

    if (!db.commit()) {
        db.close();
        QSqlDatabase::removeDatabase("i18n_viewer_submit");
        if (error) *error = db.lastError().text();
        return false;
    }

    db.close();
    QSqlDatabase::removeDatabase("i18n_viewer_submit");

    m_original = m_rows;
    qDebug() << "Submitted" << updated << "changes";
    return true;
}

void TranslationModel::revertChanges()
{
    m_rows = m_original;
    beginResetModel();
    rebuildVisible();
    endResetModel();
}

bool TranslationModel::deleteLanguage(const QString &lang, QString *error)
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "i18n_viewer_delete");
    db.setDatabaseName(m_dbPath);

    if (!db.open()) {
        if (error) *error = db.lastError().text();
        return false;
    }

    db.transaction();

    // Delete from i18n_message
    QSqlQuery q1(db);
    q1.prepare("DELETE FROM i18n_message WHERE lang = ?");
    q1.bindValue(0, lang);
    if (!q1.exec()) {
        db.rollback();
        db.close();
        QSqlDatabase::removeDatabase("i18n_viewer_delete");
        if (error) *error = q1.lastError().text();
        return false;
    }

    // Delete from i18n_language
    QSqlQuery q2(db);
    q2.prepare("DELETE FROM i18n_language WHERE lang = ?");
    q2.bindValue(0, lang);
    if (!q2.exec()) {
        db.rollback();
        db.close();
        QSqlDatabase::removeDatabase("i18n_viewer_delete");
        if (error) *error = q2.lastError().text();
        return false;
    }

    if (!db.commit()) {
        db.close();
        QSqlDatabase::removeDatabase("i18n_viewer_delete");
        if (error) *error = db.lastError().text();
        return false;
    }

    db.close();
    QSqlDatabase::removeDatabase("i18n_viewer_delete");

    return reload(error);
}

QMap<QString, TranslationModel::Stat> TranslationModel::stats() const
{
    QMap<QString, Stat> result;
    for (const TranslationRow &r : m_rows) {
        Stat &s = result[r.lang];
        ++s.total;
        if (r.msgstr.isEmpty())
            ++s.empty;
    }
    return result;
}

const TranslationRow *TranslationModel::rowAt(int row) const
{
    if (row < 0 || row >= m_visible.size())
        return nullptr;
    return &m_rows[m_visible[row]];
}
