#include "SerialPortManager.h"

SerialPortManager::SerialPortManager() {
  connect(&mSerial, &QSerialPort::errorOccurred, this,
          &SerialPortManager::handleError);
  connect(&mSerial, &QSerialPort::readyRead, this,
          &SerialPortManager::readData);
}

bool SerialPortManager::openSerialPort(
    const SettingsDialog::Settings& settings) {
  mSerial.setPortName(settings.name);
  mSerial.setBaudRate(settings.baudRate);
  mSerial.setDataBits(settings.dataBits);
  mSerial.setParity(settings.parity);
  mSerial.setStopBits(settings.stopBits);
  mSerial.setFlowControl(settings.flowControl);
  if (mSerial.open(QIODevice::ReadWrite)) {
    return true;
  } else {
    emit serialPortError(QStringList(mSerial.errorString()));
  }
  return false;
}

void SerialPortManager::closeSerialPort() {
  if (mSerial.isOpen()) mSerial.close();
}

void SerialPortManager::writeCommands(const QList<QByteArray>& commandsList) {
  mErrorBuffer.clear();
  for (auto& command : commandsList) {
    if (!mErrorBuffer.isEmpty()) {
      return;
    }
    QByteArray data("\x1bP");
    data += command;
    data += getCheckSum(command);
    data += "\x1b\\";
    data += '\x10';
    writeData(data);
  }
}

char SerialPortManager::getCheckSum(const QByteArray& data) {
  char out = static_cast<char>(0xFF);
  for (char ch : data) {
    out = out ^ ch;
  }
  return out;
}

void SerialPortManager::writeData(const QByteArray& data) {
  qint64 numOfBytesWritten = mSerial.write(data);
  if (numOfBytesWritten != data.size()) {
    emit serialPortError(
        QStringList(QString("Did not write all bytes. Wrote %1 byte(s) from %2.")
            .arg(numOfBytesWritten)
            .arg(data.size())));
  }
  if(mSerial.waitForReadyRead(1000)){
      readData();
  }
}

bool SerialPortManager::isPortOpen() { return mSerial.isOpen(); }

void SerialPortManager::readData() {
  const QByteArray data = mSerial.readAll();
  char key = static_cast<char>(0b01110000);
  char mask = static_cast<char>(0b11111000);
  QStringList errors;
  for (auto& byte : data) {
    if ((byte & mask) == key) {
      if (byte & 0x1) errors.append("Błąd mechanizmu drukującego");
      if (byte & 0x2) errors.append("Brak papieru");
    }
  }
  if(!errors.isEmpty()){
      mErrorBuffer.append(errors);
      emit serialPortError(errors);
  }
}

void SerialPortManager::handleError(QSerialPort::SerialPortError error) {
  if (error != QSerialPort::NoError) {
    if (error == QSerialPort::ResourceError) {
      closeSerialPort();
    }
    emit serialPortError(QStringList(mSerial.errorString()));
  }
}
