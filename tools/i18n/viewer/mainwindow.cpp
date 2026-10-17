#include "mainwindow.h"
#include "translationmodel.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QTableView>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_model = new TranslationModel(this);

    createActions();
    createToolBar();
    createCentralWidget();

    m_statusLabel = new QLabel(tr("No database loaded"), this);
    statusBar()->addWidget(m_statusLabel);

    resize(1200, 700);
    updateWindowTitle();
}

void MainWindow::createActions()
{
    m_openAction = new QAction(tr("Open..."), this);
    m_openAction->setShortcut(QKeySequence::Open);
    connect(m_openAction, &QAction::triggered, this, &MainWindow::onOpen);

    m_saveAction = new QAction(tr("Save"), this);
    m_saveAction->setShortcut(QKeySequence::Save);
    m_saveAction->setEnabled(false);
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::onSave);

    m_revertAction = new QAction(tr("Revert"), this);
    m_revertAction->setEnabled(false);
    connect(m_revertAction, &QAction::triggered, this, &MainWindow::onRevert);

    m_reloadAction = new QAction(tr("Reload"), this);
    m_reloadAction->setShortcut(QKeySequence::Refresh);
    m_reloadAction->setEnabled(false);
    connect(m_reloadAction, &QAction::triggered, this, &MainWindow::onReload);

    m_deleteLangAction = new QAction(tr("Delete language..."), this);
    m_deleteLangAction->setEnabled(false);
    connect(m_deleteLangAction, &QAction::triggered, this, &MainWindow::onDeleteLanguage);

    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(m_openAction);
    fileMenu->addAction(m_saveAction);
    fileMenu->addAction(m_revertAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_reloadAction);
    fileMenu->addAction(m_deleteLangAction);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), this, &QWidget::close, QKeySequence::Quit);
}

void MainWindow::createToolBar()
{
    QToolBar *bar = addToolBar(tr("Main"));
    bar->setMovable(false);

    bar->addAction(m_openAction);
    bar->addAction(m_saveAction);
    bar->addAction(m_revertAction);
    bar->addAction(m_reloadAction);
    bar->addSeparator();

    bar->addWidget(new QLabel(tr(" Language: "), this));
    m_languageCombo = new QComboBox(this);
    m_languageCombo->setMinimumWidth(140);
    connect(m_languageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onLanguageFilterChanged);
    bar->addWidget(m_languageCombo);

    bar->addWidget(new QLabel(tr("  Search: "), this));
    m_textFilterEdit = new QLineEdit(this);
    m_textFilterEdit->setPlaceholderText(tr("context / source / translation"));
    m_textFilterEdit->setClearButtonEnabled(true);
    m_textFilterEdit->setMinimumWidth(220);
    connect(m_textFilterEdit, &QLineEdit::textChanged,
            this, &MainWindow::onTextFilterChanged);
    bar->addWidget(m_textFilterEdit);

    m_onlyEmptyCheck = new QCheckBox(tr("Only untranslated"), this);
    connect(m_onlyEmptyCheck, &QCheckBox::toggled,
            this, &MainWindow::onOnlyEmptyToggled);
    bar->addWidget(m_onlyEmptyCheck);
}

void MainWindow::createCentralWidget()
{
    QWidget *central = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(central);
    layout->setContentsMargins(4, 4, 4, 4);

    m_table = new QTableView(central);
    m_table->setModel(m_model);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->setSortingEnabled(false);
    m_table->verticalHeader()->setVisible(false);

    QHeaderView *h = m_table->horizontalHeader();
    h->setSectionResizeMode(TranslationModel::ColLang, QHeaderView::ResizeToContents);
    h->setSectionResizeMode(TranslationModel::ColCtx, QHeaderView::Interactive);
    h->setSectionResizeMode(TranslationModel::ColMsgid, QHeaderView::Stretch);
    h->setSectionResizeMode(TranslationModel::ColForm, QHeaderView::ResizeToContents);
    h->setSectionResizeMode(TranslationModel::ColMsgstr, QHeaderView::Stretch);

    connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &MainWindow::onSelectionChanged);

    layout->addWidget(m_table, 1);

    m_detailLabel = new QLabel(central);
    m_detailLabel->setWordWrap(true);
    m_detailLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_detailLabel->setMinimumHeight(48);
    layout->addWidget(m_detailLabel);

    setCentralWidget(central);
}

void MainWindow::onOpen()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open translation database"), QString(),
        tr("SQLite database (*.db *.sqlite *.sqlite3);;All files (*)"));
    if (path.isEmpty())
        return;

    if (!confirmDiscardChanges())
        return;

    QString error;
    if (!m_model->openDatabase(path, &error)) {
        QMessageBox::critical(this, tr("Cannot open database"),
                              tr("Failed to open %1:\n%2").arg(path, error));
        return;
    }

    m_dbPath = path;
    refreshLanguageCombo();
    updateWindowTitle();
    updateStatusBar();

    m_saveAction->setEnabled(false);
    m_revertAction->setEnabled(false);
    m_reloadAction->setEnabled(true);
    m_deleteLangAction->setEnabled(true);
}

void MainWindow::onSave()
{
    QString error;
    if (!m_model->submitChanges(&error)) {
        QMessageBox::critical(this, tr("Save failed"), error);
        return;
    }

    m_saveAction->setEnabled(false);
    m_revertAction->setEnabled(false);
    updateStatusBar();
    statusBar()->showMessage(tr("Changes saved"), 3000);
}

void MainWindow::onRevert()
{
    if (!m_model->hasChanges())
        return;

    const auto reply = QMessageBox::question(
        this, tr("Revert changes"),
        tr("Discard %1 unsaved change(s)?").arg(m_model->dirtyCount()));
    if (reply != QMessageBox::Yes)
        return;

    m_model->revertChanges();
    m_saveAction->setEnabled(false);
    m_revertAction->setEnabled(false);
    updateStatusBar();
}

void MainWindow::onReload()
{
    if (!confirmDiscardChanges())
        return;

    QString error;
    if (!m_model->reload(&error)) {
        QMessageBox::critical(this, tr("Reload failed"), error);
        return;
    }

    refreshLanguageCombo();
    updateStatusBar();

    m_saveAction->setEnabled(false);
    m_revertAction->setEnabled(false);
}

void MainWindow::onDeleteLanguage()
{
    const QString lang = m_languageCombo->currentData().toString();
    if (lang.isEmpty())
        return;

    const auto reply = QMessageBox::warning(
        this, tr("Delete language"),
        tr("Delete every translation of language '%1' from the database?\n"
           "This cannot be undone.").arg(lang),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes)
        return;

    QString error;
    if (!m_model->deleteLanguage(lang, &error)) {
        QMessageBox::critical(this, tr("Delete failed"), error);
        return;
    }

    refreshLanguageCombo();
    updateStatusBar();
    statusBar()->showMessage(tr("Language '%1' deleted").arg(lang), 3000);
}

void MainWindow::onLanguageFilterChanged(int index)
{
    Q_UNUSED(index)
    if (!m_model)
        return;
    m_model->setLanguageFilter(m_languageCombo->currentData().toString());
    updateStatusBar();
}

void MainWindow::onTextFilterChanged(const QString &text)
{
    m_model->setTextFilter(text);
    updateStatusBar();
}

void MainWindow::onOnlyEmptyToggled(bool checked)
{
    m_model->setOnlyEmpty(checked);
    updateStatusBar();
}

void MainWindow::onSelectionChanged()
{
    const QModelIndexList rows = m_table->selectionModel()->selectedRows();
    if (rows.isEmpty()) {
        m_detailLabel->clear();
        return;
    }

    const TranslationRow *r = m_model->rowAt(rows.first().row());
    if (!r) {
        m_detailLabel->clear();
        return;
    }

    QString text = QString("<b>%1</b>").arg(r->lang);
    if (!r->ctx.isEmpty())
        text += QString(" &nbsp;|&nbsp; context: <i>%1</i>").arg(r->ctx);
    text += QString(" &nbsp;|&nbsp; form: %1").arg(r->form);
    if (!r->pluralRule.isEmpty())
        text += QString(" &nbsp;|&nbsp; rule: <code>%1</code>").arg(r->pluralRule);

    // Show a plain-text rendering of the translation, newlines preserved
    QString preview = r->msgstr;
    preview.replace("\n", "\u21b5");

    m_detailLabel->setText(text + QString("<br><br>%1").arg(
        preview.isEmpty() ? tr("<i>(untranslated)</i>") : preview.toHtmlEscaped()));
}

void MainWindow::refreshLanguageCombo()
{
    const QString previous = m_languageCombo->currentData().toString();

    m_languageCombo->blockSignals(true);
    m_languageCombo->clear();
    m_languageCombo->addItem(tr("(all languages)"), QString());
    for (const QString &lang : m_model->languages())
        m_languageCombo->addItem(lang, lang);

    // Restore the previous selection when it still exists
    int restore = 0;
    if (!previous.isEmpty()) {
        const int idx = m_languageCombo->findData(previous);
        if (idx >= 0)
            restore = idx;
    }
    m_languageCombo->setCurrentIndex(restore);
    m_languageCombo->blockSignals(false);

    m_model->setLanguageFilter(m_languageCombo->currentData().toString());
}

void MainWindow::updateStatusBar()
{
    if (!m_model)
        return;

    const int total = m_model->rowCount();
    const int dirty = m_model->dirtyCount();

    QString text = tr("%1 row(s)").arg(total);
    if (dirty > 0)
        text += tr("  |  %1 unsaved change(s)").arg(dirty);

    m_statusLabel->setText(text);

    // Per-language summary
    QStringList parts;
    const auto st = m_model->stats();
    for (auto it = st.constBegin(); it != st.constEnd(); ++it) {
        const int done = it.value().total - it.value().empty;
        parts << QString("%1 %2/%3").arg(it.key()).arg(done).arg(it.value().total);
    }
    if (!parts.isEmpty())
        text += "  |  " + parts.join(", ");
    m_statusLabel->setText(text);

    // Enable save/revert only when there is something to save
    const bool hasChanges = dirty > 0;
    m_saveAction->setEnabled(hasChanges);
    m_revertAction->setEnabled(hasChanges);
}

void MainWindow::updateWindowTitle()
{
    if (m_dbPath.isEmpty()) {
        setWindowTitle(tr("i18n Translation Viewer"));
        return;
    }
    setWindowTitle(tr("i18n Translation Viewer - %1").arg(QFileInfo(m_dbPath).fileName()));
}

bool MainWindow::confirmDiscardChanges()
{
    if (!m_model->hasChanges())
        return true;

    const auto reply = QMessageBox::question(
        this, tr("Unsaved changes"),
        tr("There are %1 unsaved change(s). Discard them?")
            .arg(m_model->dirtyCount()),
        QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel);

    return reply == QMessageBox::Discard;
}
