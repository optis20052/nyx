#include "AddServiceDialog.h"
#include "utils/Constants.h"

#include <QCheckBox>
#include <QComboBox>
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
#include <QProcess>
#include <QPushButton>
#include <QVBoxLayout>

Q_LOGGING_CATEGORY(lcAddServiceDialog, "nyx.ui.addservicedialog")

namespace nyx::ui {

AddServiceDialog::AddServiceDialog(QWidget *parent)
    : QDialog(parent)
{
    loadAvailableServices();
    initUi();
}

void AddServiceDialog::initUi()
{
    setWindowTitle(QStringLiteral("Add Service"));
    setMinimumWidth(450);

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout();

    // Service combo
    m_serviceCombo = new QComboBox();
    m_serviceCombo->setEditable(true);
    m_serviceCombo->addItem(QStringLiteral("-- Select a service --"), QVariant());

    QStringList keys = m_availableServices.keys();
    keys.sort();
    for (const QString &displayName : keys) {
        const auto &pair = m_availableServices[displayName];
        m_serviceCombo->addItem(displayName, QVariant::fromValue(pair));
    }
    connect(m_serviceCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &AddServiceDialog::onServiceSelected);
    form->addRow(QStringLiteral("Select Service*:"), m_serviceCombo);

    // Service name (read-only)
    m_nameInput = new QLineEdit();
    m_nameInput->setReadOnly(true);
    m_nameInput->setPlaceholderText(QStringLiteral("Select a service from the dropdown above"));
    form->addRow(QStringLiteral("Service Name:"), m_nameInput);

    // Display name
    m_displayNameInput = new QLineEdit();
    m_displayNameInput->setPlaceholderText(QStringLiteral("User-friendly name (optional)"));
    form->addRow(QStringLiteral("Display Name:"), m_displayNameInput);

    // Icons
    auto *iconsGroup = new QVBoxLayout();
    auto *iconsLabel = new QLabel(QStringLiteral("Service Icons:"));
    iconsLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    iconsGroup->addWidget(iconsLabel);

    auto *iconsContainer = new QHBoxLayout();

    // Light theme icon
    auto *lightLayout = new QVBoxLayout();
    auto *lightLabel = new QLabel(QStringLiteral("Light Theme Icon\n(use dark-colored icon)"));
    lightLabel->setAlignment(Qt::AlignCenter);
    lightLayout->addWidget(lightLabel);

    m_lightIconPreview = new QLabel();
    m_lightIconPreview->setFixedSize(64, 64);
    m_lightIconPreview->setStyleSheet(QStringLiteral("background-color: #F5F5F5; border: 1px solid #ccc; border-radius: 4px;"));
    m_lightIconPreview->setScaledContents(true);
    lightLayout->addWidget(m_lightIconPreview, 0, Qt::AlignCenter);

    m_lightIconInput = new QLineEdit(QStringLiteral("application-x-executable"));
    m_lightIconInput->setPlaceholderText(QStringLiteral("Icon name or path"));
    connect(m_lightIconInput, &QLineEdit::textChanged, this, [this]() { updateIconPreview(QStringLiteral("light")); });
    lightLayout->addWidget(m_lightIconInput);

    auto *lightBrowseBtn = new QPushButton(QStringLiteral("Browse..."));
    connect(lightBrowseBtn, &QPushButton::clicked, this, [this]() { browseIcon(QStringLiteral("light")); });
    lightLayout->addWidget(lightBrowseBtn);
    iconsContainer->addLayout(lightLayout);

    // Dark theme icon
    auto *darkLayout = new QVBoxLayout();
    auto *darkLabel = new QLabel(QStringLiteral("Dark Theme Icon\n(use light-colored icon)"));
    darkLabel->setAlignment(Qt::AlignCenter);
    darkLayout->addWidget(darkLabel);

    m_darkIconPreview = new QLabel();
    m_darkIconPreview->setFixedSize(64, 64);
    m_darkIconPreview->setStyleSheet(QStringLiteral("background-color: #2D2D2D; border: 1px solid #555; border-radius: 4px;"));
    m_darkIconPreview->setScaledContents(true);
    darkLayout->addWidget(m_darkIconPreview, 0, Qt::AlignCenter);

    m_darkIconInput = new QLineEdit(QStringLiteral("application-x-executable"));
    m_darkIconInput->setPlaceholderText(QStringLiteral("Icon name or path"));
    connect(m_darkIconInput, &QLineEdit::textChanged, this, [this]() { updateIconPreview(QStringLiteral("dark")); });
    darkLayout->addWidget(m_darkIconInput);

    auto *darkBrowseBtn = new QPushButton(QStringLiteral("Browse..."));
    connect(darkBrowseBtn, &QPushButton::clicked, this, [this]() { browseIcon(QStringLiteral("dark")); });
    darkLayout->addWidget(darkBrowseBtn);
    iconsContainer->addLayout(darkLayout);

    iconsGroup->addLayout(iconsContainer);
    form->addRow(QString(), iconsGroup);

    updateIconPreview(QStringLiteral("light"));
    updateIconPreview(QStringLiteral("dark"));

    // Service type (read-only)
    m_serviceTypeInput = new QLineEdit();
    m_serviceTypeInput->setReadOnly(true);
    m_serviceTypeInput->setPlaceholderText(QStringLiteral("Auto-detected"));
    form->addRow(QStringLiteral("Service Type:"), m_serviceTypeInput);

    // Checkboxes
    m_autoStartCheckbox = new QCheckBox(QStringLiteral("Start service when app launches"));
    form->addRow(QString(), m_autoStartCheckbox);

    m_enabledCheckbox = new QCheckBox(QStringLiteral("Show tray icon"));
    m_enabledCheckbox->setChecked(true);
    form->addRow(QString(), m_enabledCheckbox);

    layout->addLayout(form);
    layout->addSpacing(20);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &AddServiceDialog::onAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void AddServiceDialog::loadAvailableServices()
{
    auto loadUnits = [this](const QStringList &args, const QString &type) {
        QProcess proc;
        proc.start(QStringLiteral("systemctl"), args);
        if (!proc.waitForFinished(5000)) return;

        const QString output = QString::fromUtf8(proc.readAllStandardOutput());
        const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            const QStringList parts = line.simplified().split(QLatin1Char(' '));
            if (parts.isEmpty()) continue;
            QString name = parts[0];
            name.remove(QStringLiteral(".service"));
            const QString display = QStringLiteral("%1 (%2)").arg(name, type);
            m_availableServices[display] = {name, type};
        }
    };

    loadUnits({QStringLiteral("--user"), QStringLiteral("list-unit-files"),
               QStringLiteral("--type=service"), QStringLiteral("--no-pager"),
               QStringLiteral("--no-legend")},
              QStringLiteral("user"));

    loadUnits({QStringLiteral("list-unit-files"), QStringLiteral("--type=service"),
               QStringLiteral("--no-pager"), QStringLiteral("--no-legend")},
              QStringLiteral("system"));

    qCInfo(lcAddServiceDialog) << "Loaded" << m_availableServices.size() << "available services";
}

void AddServiceDialog::onServiceSelected(int index)
{
    if (index <= 0) {
        m_nameInput->clear();
        m_serviceTypeInput->clear();
        m_displayNameInput->clear();
        return;
    }

    const QVariant data = m_serviceCombo->currentData();
    if (!data.isValid()) return;

    const auto pair = data.value<QPair<QString, QString>>();
    m_nameInput->setText(pair.first);
    m_serviceTypeInput->setText(pair.second);

    // Generate nice display name
    QString display = pair.first.split(QLatin1Char('@')).first();
    display.replace(QLatin1Char('-'), QLatin1Char(' '));
    // Title case
    QStringList words = display.split(QLatin1Char(' '));
    for (QString &w : words) {
        if (!w.isEmpty()) w[0] = w[0].toUpper();
    }
    m_displayNameInput->setText(words.join(QLatin1Char(' ')));
}

void AddServiceDialog::updateIconPreview(const QString &iconType)
{
    QString path;
    QLineEdit *input;
    QLabel *preview;

    if (iconType == QLatin1String("light")) {
        path = m_lightIconPath;
        input = m_lightIconInput;
        preview = m_lightIconPreview;
    } else {
        path = m_darkIconPath;
        input = m_darkIconInput;
        preview = m_darkIconPreview;
    }

    QIcon icon;
    if (!path.isEmpty() && QFileInfo::exists(path)) {
        icon = QIcon(path);
    } else {
        QString value = input->text().trimmed();
        if (value.isEmpty()) value = QStringLiteral("application-x-executable");
        icon = QIcon::fromTheme(value, QIcon::fromTheme(QStringLiteral("application-x-executable")));
    }
    preview->setPixmap(icon.pixmap(64, 64));
}

void AddServiceDialog::browseIcon(const QString &iconType)
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
        qCInfo(lcAddServiceDialog) << "Copied" << iconType << "icon to" << destPath;
    } else {
        QMessageBox::warning(this, QStringLiteral("Icon Copy Failed"),
                             QStringLiteral("Could not copy icon file."));
    }
}

void AddServiceDialog::onAccept()
{
    const QString name = m_nameInput->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Invalid Input"),
                             QStringLiteral("Please select a service from the dropdown."));
        return;
    }

    QString displayName = m_displayNameInput->text().trimmed();
    if (displayName.isEmpty())
        displayName = name.at(0).toUpper() + name.mid(1);

    QString iconLight = m_lightIconPath.isEmpty() ? m_lightIconInput->text().trimmed() : m_lightIconPath;
    if (iconLight.isEmpty()) iconLight = QStringLiteral("application-x-executable");

    QString iconDark = m_darkIconPath.isEmpty() ? m_darkIconInput->text().trimmed() : m_darkIconPath;
    if (iconDark.isEmpty()) iconDark = QStringLiteral("application-x-executable");

    QString serviceType = m_serviceTypeInput->text().trimmed();
    if (serviceType.isEmpty()) serviceType = QStringLiteral("user");

    m_serviceConfig.name = name;
    m_serviceConfig.displayName = displayName;
    m_serviceConfig.icon = iconLight;
    m_serviceConfig.iconLight = (iconLight != m_serviceConfig.icon) ? iconLight : QString();
    m_serviceConfig.iconDark = (iconDark != m_serviceConfig.icon) ? iconDark : QString();
    m_serviceConfig.serviceType = serviceType;
    m_serviceConfig.autoStart = m_autoStartCheckbox->isChecked();
    m_serviceConfig.enabled = m_enabledCheckbox->isChecked();

    if (!m_serviceConfig.validate()) {
        QMessageBox::critical(this, QStringLiteral("Invalid Configuration"),
                              QStringLiteral("Failed to create service configuration."));
        return;
    }

    qCInfo(lcAddServiceDialog) << "Created service config for" << name;
    accept();
}

} // namespace nyx::ui
