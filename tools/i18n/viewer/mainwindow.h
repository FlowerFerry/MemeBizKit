#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>

class QLineEdit;
class QComboBox;
class QCheckBox;
class QTableView;
class QLabel;
class QAction;
class TranslationModel;

/**
 * Main window: opens a translation database and shows its messages in a table.
 *
 * Layout:
 *   - toolbar: open / save / revert, language filter, text filter, "only empty"
 *   - central table: language, context, source, form, translation
 *   - status bar: row counts and a per-language summary
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void onOpen();
    void onSave();
    void onRevert();
    void onReload();
    void onDeleteLanguage();
    void onLanguageFilterChanged(int index);
    void onTextFilterChanged(const QString &text);
    void onOnlyEmptyToggled(bool checked);
    void onSelectionChanged();

private:
    void createActions();
    void createToolBar();
    void createCentralWidget();
    void updateStatusBar();
    void updateWindowTitle();
    bool confirmDiscardChanges();   //!< true when it is safe to drop edits
    void refreshLanguageCombo();

    TranslationModel *m_model = nullptr;

    QComboBox   *m_languageCombo = nullptr;
    QLineEdit   *m_textFilterEdit = nullptr;
    QCheckBox   *m_onlyEmptyCheck = nullptr;
    QTableView  *m_table = nullptr;
    QLabel      *m_detailLabel = nullptr;
    QLabel      *m_statusLabel = nullptr;

    QString      m_dbPath;

    QAction     *m_openAction = nullptr;
    QAction     *m_saveAction = nullptr;
    QAction     *m_revertAction = nullptr;
    QAction     *m_reloadAction = nullptr;
    QAction     *m_deleteLangAction = nullptr;
};

#endif // MAINWINDOW_H
