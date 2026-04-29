#include "EditServiceDialog.h"
#include "utils/Constants.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

Q_LOGGING_CATEGORY(lcEditServiceDialog, "nyx.ui.editservicedialog")

namespace nyx::ui {

EditServiceDialog::EditServiceDialog(const nyx::models::ServiceConfig &config, QWidget *parent)
    : QDialog(parent)
    , m_originalConfig(config)
{
    initUi();
    populateFields();
}

void EditServiceDialog::initUi()
{
    setWindowTitle(QStringLiteral("Edit Service - %1").arg(m_originalConfig.displayName));
    setMinimumWidth(450);

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout();

    m_nameInput = new QLineEdit();
    m_nameInput->setReadOnly(true);
    form->addRow(QStringLiteral("Service Name:"), m_nameInput);

    m_displayNameInput = new QLineEdit();
    form->addRow(QStringLiteral("Display Name*:"), m_displayNameInput);

    // Icons
    auto *iconsGroup = new QVBoxLayout();
    auto *iconsLabel = new QLabel(QStringLiteral("Service Icons:"));
    iconsLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    iconsGroup->addWidget(iconsLabel);
    auto *iconsContainer = new QHBoxLayout();

    // Light
    auto *lightLayout = new QVBoxLayout();
    auto *lightLabel = new QLabel(QStringLiteral("Light Theme Icon\n(use dark-colored icon)"));
    lightLabel->setAlignment(Qt::AlignCenter);
    lightLayout->addWidget(lightLabel);
    m_lightIconPreview = new QLabel();
    m_lightIconPreview->setFixedSize(64, 64);
    m_lightIconPreview->setStyleSheet(QStringLiteral("background-color: #F5F5F5; border: 1px solid #ccc; border-radius: 4px;"));
    m_lightIconPreview->setScaledContents(true);
    lightLayout->addWidget(m_lightIconPreview, 0, Qt::AlignCenter);
    m_lightIconInput = new QLineEdit();
    m_lightIconInput->setPlaceholderText(QStringLiteral("Icon name or path"));
    connect(m_lightIconInput, &QLineEdit::textChanged, this, [this]() { updateIconPreview(QStringLiteral("light")); });
    lightLayout->addWidget(m_lightIconInput);
    auto *lightBrowse = new QPushButton(QStringLiteral("Browse..."));
    connect(lightBrowse, &QPushButton::clicked, this, [this]() { browseIcon(QStringLiteral("light")); });
    lightLayout->addWidget(lightBrowse);
    iconsContainer->addLayout(lightLayout);

    // Dark
    auto *darkLayout = new QVBoxLayout();
    auto *darkLabel = new QLabel(QStringLiteral("Dark Theme Icon\n(use light-colored icon)"));
    darkLabel->setAlignment(Qt::AlignCenter);
    darkLayout->addWidget(darkLabel);
    m_darkIconPreview = new QLabel();
    m_darkIconPreview->setFixedSize(64, 64);
    m_darkIconPreview->setStyleSheet(QStringLiteral("background-color: #2D2D2D; border: 1px solid #555; border-radius: 4px;"));
    m_darkIconPreview->setScaledContents(true);
    darkLayout->addWidget(m_darkIconPreview, 0, Qt::AlignCenter);
    m_darkIconInput = new QLineEdit();
    m_darkIconInput->setPlaceholderText(QStringLiteral("Icon name or path"));
    connect(m_darkIconInput, &QLineEdit::textChanged, this, [this]() { updateIconPreview(QStringLiteral("dark")); });
    darkLayout->addWidget(m_darkIconInput);
    auto *darkBrowse = new QPushButton(QStringLiteral("Browse..."));
    connect(darkBrowse, &QPushButton::clicked, this, [this]() { browseIcon(QStringLiteral("dark")); });
    darkLayout->addWidget(darkBrowse);
    iconsContainer->addLayout(darkLayout);

    iconsGroup->addLayout(iconsContainer);
    form->addRow(QString(), iconsGroup);

    m_serviceTypeInput = new QLineEdit();
    m_serviceTypeInput->setReadOnly(true);
    form->addRow(QStringLiteral("Service Type:"), m_serviceTypeInput);

    m_autoStartCheckbox = new QCheckBox(QStringLiteral("Start service when app launches"));
    form->addRow(QString(), m_autoStartCheckbox);

    m_enabledCheckbox = new QCheckBox(QStringLiteral("Show tray icon"));
    form->addRow(QString(), m_enabledCheckbox);

    layout->addLayout(form);
    layout->addSpacing(20);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &EditServiceDialog::onAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void EditServiceDialog::populateFields()
{
    m_nameInput->setText(m_originalConfig.name);
    m_displayNameInput->setText(m_originalConfig.displayName);
    m_serviceTypeInput->setText(m_originalConfig.serviceType);
    m_autoStartCheckbox->setChecked(m_originalConfig.autoStart);
    m_enabledCheckbox->setChecked(m_originalConfig.enabled);

    // Light icon
    auto setupIcon = [](const QString &themeIcon, const QString &baseIcon,
                        QLineEdit *input, QString &storedPath) {
        const QString &src = themeIcon.isEmpty() ? baseIcon : themeIcon;
        if (!src.isEmpty() && (src.startsWith(QLatin1Char('/')) || src.startsWith(QLatin1String("./")))) {
            storedPath = src;
            input->setText(QFileInfo(src).fileName());
        } else {
            input->setText(src);
        }
    };

    setupIcon(m_originalConfig.iconLight, m_originalConfig.icon, m_lightIconInput, m_lightIconPath);
    setupIcon(m_originalConfig.iconDark, m_originalConfig.icon, m_darkIconInput, m_darkIconPath);

    updateIconPreview(QStringLiteral("light"));
    updateIconPreview(QStringLiteral("dark"));
}

void EditServiceDialog::updateIconPreview(const QString &iconType)
{
    QString path;
    QLineEdit *input;
    QLabel *preview;

    if (iconType == QLatin1String("light")) {
        path = m_lightIconPath; input = m_lightIconInput; preview = m_lightIconPreview;
    } else {
        path = m_darkIconPath; input = m_darkIconInput; preview = m_darkIconPreview;
    }

    QIcon icon;
    if (!path.isEmpty() && QFileInfo::exists(path)) {
        icon = QIcon(path);
    } else {
        QString val = input->text().trimmed();
        if (val.isEmpty()) val = QStringLiteral("application-x-executable");
        icon = QIcon::fromTheme(val, QIcon::fromTheme(QStringLiteral("application-x-executable")));
    }
    preview->setPixmap(icon.pixmap(64, 64));
}

void EditServiceDialog::browseIcon(const QString &iconType)
{
    const QString title = QStringLiteral("Select %1 Theme Icon").arg(iconType.at(0).toUpper() + iconType.mid(1));
    const QString filePath = QFileDialog::getOpenFileName(
        this, title, QString(),
        QStringLiteral("Image Files (*.png *.svg *.ico *.xpm);;All Files (*)"));

    if (filePath.isEmpty()) return;

    const QFileInfo srcInfo(filePath);
    const QString destDir = nyx::utils::iconsDir();
    QDir().mkpath(destDir);

    const QString destName = QStringLiteral("icon_%1_%2%3")
                                 .arg(iconType)
                                 .arg(QDateTime::currentMSecsSinceEpoch())
                                 .arg(srcInfo.suffix().isEmpty() ? QString() : QLatin1Char('.') + srcInfo.suffix());
    const QString destPath = destDir + QLatin1Char('/') + destName;

    if (QFile::copy(filePath, destPath)) {
        if (iconType == QLatin1String("light")) {
            m_lightIconPath = destPath;
            m_lightIconInput->setText(srcInfo.fileName());
        } else {
            m_darkIconPath = destPath;
            m_darkIconInput->setText(srcInfo.fileName());
        }
        updateIconPreview(iconType);
    } else {
        QMessageBox::warning(this, QStringLiteral("Icon Copy Failed"),
                             QStringLiteral("Could not copy icon file."));
    }
}

void EditServiceDialog::onAccept()
{
    const QString displayName = m_displayNameInput->text().trimmed();
    if (displayName.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Invalid Input"),
                             QStringLiteral("Display name is required."));
        return;
    }

    QString iconLight = m_lightIconPath.isEmpty() ? m_lightIconInput->text().trimmed() : m_lightIconPath;
    if (iconLight.isEmpty()) iconLight = QStringLiteral("application-x-executable");

    QString iconDark = m_darkIconPath.isEmpty() ? m_darkIconInput->text().trimmed() : m_darkIconPath;
    if (iconDark.isEmpty()) iconDark = QStringLiteral("application-x-executable");

    m_updatedConfig = m_originalConfig;
    m_updatedConfig.displayName = displayName;
    m_updatedConfig.icon = iconLight;
    m_updatedConfig.iconLight = (iconLight != m_updatedConfig.icon) ? iconLight : QString();
    m_updatedConfig.iconDark = (iconDark != m_updatedConfig.icon) ? iconDark : QString();
    m_updatedConfig.autoStart = m_autoStartCheckbox->isChecked();
    m_updatedConfig.enabled = m_enabledCheckbox->isChecked();

    if (!m_updatedConfig.validate()) {
        QMessageBox::critical(this, QStringLiteral("Invalid Configuration"),
                              QStringLiteral("Failed to update service configuration."));
        return;
    }

    qCInfo(lcEditServiceDialog) << "Updated service config for" << m_originalConfig.name;
    accept();
}

} // namespace nyx::ui
