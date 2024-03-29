#include "MainWindow.h"

#include <QSerialPortInfo>

#include "ui_MainWindow.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), mSerialPortManager(), ui(new Ui::MainWindow) {
  setAcceptDrops(true);
  ui->setupUi(this);
  ui->fileNameLabel->hide();
  ui->sendButton->setEnabled(false);
  ui->tableWidget->horizontalHeader()->setSectionResizeMode(
      QHeaderView::Stretch);
  makeConnections();
  if (QApplication::arguments().size() > 1) {
    openFileAndReadContent(QApplication::arguments().last());
  }
  for (const QSerialPortInfo& port : QSerialPortInfo::availablePorts()) {
    ui->portNameComboBox->addItem(port.portName());
  }
}

MainWindow::~MainWindow() {
  closeSerialPort();
  delete ui;
}

bool MainWindow::nativeEvent(const QByteArray& eventType, void* message,
                             long* result) {
  if (eventType == "windows_generic_MSG") {
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_COPYDATA) {
      COPYDATASTRUCT* pcds = reinterpret_cast<COPYDATASTRUCT*>(msg->lParam);
      if (pcds->dwData == copydataIdentifier) {
        this->showNormal();
        this->activateWindow();
        QString str = reinterpret_cast<char*>(pcds->lpData);
        QUrl url = QUrl::fromUserInput(str);
        openFileAndReadContent(url.toLocalFile());
      }
    }
  }
  return QMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
  QString filePath = event->mimeData()->urls().first().toLocalFile();
  if (filePath.endsWith("rct")) event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event) {
  QString filePath = event->mimeData()->urls().first().toLocalFile();
  openFileAndReadContent(filePath);
}

bool MainWindow::openSerialPort() {
  if (mSerialPortManager.openSerialPort(ui->portNameComboBox->currentText())) {
    showStatusMessage(tr("Połączono"));
    return true;
  } else {
    showStatusMessage(tr("Błąd połączenia"));
    return false;
  }
}

void MainWindow::closeSerialPort() { mSerialPortManager.closeSerialPort(); }

void MainWindow::handleError(const QStringList& errorsList) {
  QString errorsToShow;
  for (auto& error : errorsList) {
    errorsToShow += error;
    errorsToShow += '\n';
  }
  QMessageBox::critical(this, tr("Critical Error"), errorsToShow);
}

void MainWindow::showAboutDialog() {
  QMessageBox msgBox;
  msgBox.setTextFormat(Qt::RichText);
  msgBox.setWindowTitle("O programie");
  msgBox.setText(
      "SerialPortPrinter v0.7.1<br>"
      "This software is licensed under LGPLv3 License<br>"
      "Created with <a href=\"https://www.qt.io/\">Qt 5.13.1</a><br>"
      "More info under:<br>"
      "<a href=\"https://github.com/TheRealMazur/SerialPortPrinter\">"
      "https://github.com/TheRealMazur/SerialPortPrinter</a>");
  msgBox.exec();
}

void MainWindow::showAboutQtDialog() { QMessageBox::aboutQt(this); }

void MainWindow::on_fileOpenButton_released() {
  QString fileName = QFileDialog::getOpenFileName(this, tr("Wybierz plik"), "",
                                                  tr("Pliki druku (*.rct)"));
  if (!fileName.isEmpty()) {
    openFileAndReadContent(fileName);
  }
}

void MainWindow::on_sendButton_released() {
  ui->sendButton->setEnabled(false);
  if (openSerialPort()) {
    mSerialPortManager.writeCommands(mCommandList);
    closeSerialPort();
  } else {
    QMessageBox::critical(this, tr("Critical Error"),
                          "Nie udało się otworzyć portu!");
  }
  ui->sendButton->setEnabled(true);
}

void MainWindow::on_cancelButton_released() {
  if (mSerialPortManager.isPortOpen()) {
    closeSerialPort();
  }
  ui->sendButton->setEnabled(false);
  clearTableWidget();
  ui->fileNameLabel->hide();
}

void MainWindow::makeConnections() {
  connect(&mSerialPortManager, &SerialPortManager::serialPortError, this,
          &MainWindow::handleError);
  connect(ui->actionWy_cz, &QAction::triggered, this, &MainWindow::close);
  connect(ui->actionAbout_Qt, &QAction::triggered, this,
          &MainWindow::showAboutQtDialog);
  connect(ui->actionInformacje, &QAction::triggered, this,
          &MainWindow::showAboutDialog);
}

void MainWindow::showStatusMessage(const QString& message) {
  this->statusBar()->showMessage(message);
}

void MainWindow::openFileAndReadContent(const QString& fileName) {
  ui->fileNameLabel->hide();
  ui->fileNameLabel->setText(fileName);
  QFile file(fileName);
  file.open(QIODevice::ReadOnly);
  if (file.isOpen()) {
    ui->fileNameLabel->show();
    QString fileContent = file.readAll();
    file.close();
    handleOpenedFile(fileContent);

  } else {
    QMessageBox::warning(this, "Błąd pliku", "Nie udało się otworzyć pliku");
  }
}

void MainWindow::handleOpenedFile(QString& fileContent) {
  if (parseFileContent(fileContent)) {
    this->statusBar()->showMessage("Otwarto plik.");
    ui->sendButton->setEnabled(true);
  } else {
    QMessageBox::warning(this, "Błąd pliku", "Błędna zawartość pliku");
  }
}

bool MainWindow::parseFileContent(QString& fileContent) {
  fileContent.replace("<CR>", "\r");
  mFileContent = QJsonDocument::fromJson(fileContent.toLocal8Bit());
  QJsonObject jsonObject = mFileContent.object();
  if (jsonObject.keys().size() != 2) {
    return false;
  }
  getCommandsFromJson(jsonObject);
  fillTableWidgetFromJson(jsonObject);

  return true;
}

void MainWindow::getCommandsFromJson(const QJsonObject& jsonObject) {
  mCommandList.clear();
  QJsonArray dataArray = jsonObject.value("data").toArray();
  for (auto value : dataArray) {
    mCommandList.append(value.toString().toLocal8Bit());
  }
}

void MainWindow::fillTableWidgetFromJson(const QJsonObject& jsonObject) {
  auto previewJsonObject = jsonObject.value("preview").toObject();
  clearTableWidget();
  for (auto value : previewJsonObject) {
    ui->tableWidget->insertRow(ui->tableWidget->rowCount());
    int currentRow = ui->tableWidget->rowCount() - 1;

    QJsonObject previewObject = value.toObject();
    fillTableRow(currentRow, previewObject);
  }
}

void MainWindow::fillTableRow(const int& currentRow,
                              const QJsonObject& previewObject) {
  QString name = previewObject.value("name").toString();
  QString amount = previewObject.value("amount").toString();
  QString price = previewObject.value("price").toString();
  QString total = previewObject.value("total").toString();

  ui->tableWidget->setItem(currentRow, 0, new QTableWidgetItem(name));
  ui->tableWidget->setItem(currentRow, 1, new QTableWidgetItem(amount));
  ui->tableWidget->setItem(currentRow, 2, new QTableWidgetItem(price));
  ui->tableWidget->setItem(currentRow, 3, new QTableWidgetItem(total));
}

void MainWindow::clearTableWidget() { ui->tableWidget->setRowCount(0); }
