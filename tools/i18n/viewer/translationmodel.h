#ifndef TRANSLATIONMODEL_H
#define TRANSLATIONMODEL_H

#include <QAbstractTableModel>
#include <QVector>
#include <QString>
#include <QSet>

/** One row of i18n_message: a (lang, ctx, msgid, form) cell. */
struct TranslationRow
{
    QString lang;
    QString ctx;
    QString msgid;
    int     form = 0;
    QString msgstr;
    QString pluralRule;   //!< plural rule of the language, shown once per language
};

/**
 * Table model backed by a SQLite database using the schema from catalog.h:
 *
 *   i18n_language(lang TEXT PRIMARY KEY, plural_rule TEXT NOT NULL DEFAULT '')
 *   i18n_message(lang, ctx, msgid, form, msgstr,
 *                PRIMARY KEY (lang, ctx, msgid, form))
 *
 * The model is read-only; editing is done through setMsgstr() / submitChanges().
 */
class TranslationModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        ColLang = 0,
        ColCtx,
        ColMsgid,
        ColForm,
        ColMsgstr,
        ColumnCount
    };

    explicit TranslationModel(QObject *parent = nullptr);
    ~TranslationModel() override;

    /** Opens the database and loads every message row. Returns false on failure. */
    bool openDatabase(const QString &path, QString *error = nullptr);

    /** Reloads rows from the open database, keeping the current filters. */
    bool reload(QString *error = nullptr);

    /** Languages present in i18n_language, in file order. */
    QStringList languages() const { return m_languages; }

    /** Plural rule for one language (empty when unknown). */
    QString pluralRule(const QString &lang) const { return m_pluralRules.value(lang); }

    // ---- filtering ----
    void setLanguageFilter(const QString &lang);   //!< empty = all languages
    void setTextFilter(const QString &needle);     //!< matches ctx / msgid / msgstr
    void setOnlyEmpty(bool onlyEmpty);             //!< show only untranslated rows

    QString languageFilter() const { return m_languageFilter; }
    QString textFilter() const { return m_textFilter; }
    bool onlyEmpty() const { return m_onlyEmpty; }

    // ---- edit buffer ----
    /** Stage a new translation for a row. Emits dataChanged so the view updates. */
    void setMsgstr(int row, const QString &text);
    bool isDirty(int row) const
    {
        return m_rows.at(row).msgstr != m_original.at(row).msgstr;
    }
    int dirtyCount() const;
    bool hasChanges() const { return dirtyCount() > 0; }

    /** Writes every changed row back to the database in one transaction. */
    bool submitChanges(QString *error = nullptr);
    void revertChanges();

    /** Removes every row of one language from the database. */
    bool deleteLanguage(const QString &lang, QString *error = nullptr);

    // ---- statistics ----
    /** Number of rows per language, and how many of them are still empty. */
    struct Stat { int total = 0; int empty = 0; };
    QMap<QString, Stat> stats() const;

    // QAbstractTableModel
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value,
                 int role = Qt::EditRole) override;

    /** Row of the underlying array for a view row. */
    const TranslationRow *rowAt(int row) const;

private:
    void rebuildVisible();
    void clearRows();

    QString                              m_dbPath;
    QStringList                          m_languages;
    QMap<QString, QString>               m_pluralRules;
    QVector<TranslationRow>              m_rows;      //!< every row, unfiltered
    QVector<TranslationRow>              m_original;  //!< snapshot for dirty checks
    QVector<int>                         m_visible;   //!< indices into m_rows

    QString                              m_languageFilter;
    QString                              m_textFilter;
    bool                                 m_onlyEmpty = false;
};

#endif // TRANSLATIONMODEL_H
