﻿//
// Created by Anna on 2019/11/26.
//

#include "MaskGeneratorView.h"
#include <QFileDialog>
#include <QDialog>
#include <QFileInfo>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QFile>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <QGraphicsScene>
#include <QPixmap>
#include <QDateTime>
#include "Tools.h"
#include "Utils.h"
/*
 * 1. 直接打开的图片格式:png,jpg,jpeg,bmp,tif
 */

/*
 * 构造函数
 */
MaskGeneratorView::MaskGeneratorView(QWidget *parent) :QMainWindow(parent)
{
    //TODO:关键变量的初始化
	this->qimage_to_show = new QImage();
	this->history = new History();
    this->gray = new cv::Mat();
    //TODO:ui初始化和个性化
    ui.setupUi(this);
    ///绘图区域
    this->m_GraphicsView = new MyGraphicsView();
    ui.imgLayout->addWidget(m_GraphicsView);
    connect(m_GraphicsView, SIGNAL(mouseWheelZoom(int)), this, SLOT(onMouseWheelZoom(int)));
    ///历史记录窗口
    this->m_HistoryLogWidget=new HistoryLogWidget(this);
    ui.historyLayout->addWidget(m_HistoryLogWidget);
    m_HistoryLogWidget->show();
    ///状态栏
    this->m_PosLabel =new QLabel(QStringLiteral("就绪"));
    this->m_PosLabel->setMinimumSize(m_PosLabel->sizeHint());
    this->m_PosLabel->setAlignment(Qt::AlignHCenter);
    ui.statusbar->addWidget(this->m_PosLabel);
    ui.statusbar->setStyleSheet(QString("QStatusBar::item{border: 0px}"));
    ui.action_show_origin->setChecked(true);
    ///画笔按钮
    ui.pushButton_mouse->setChecked(true);

    ///接收拖放
    setAcceptDrops(true);

    //TODO:防呆
    MasterSwitch(false);
}
/*
 * private:创建json文件
 */
void MaskGeneratorView::CreateJsonList(QStringList filelist) {
    QFile file(JsonFile);
    if (!file.open(QIODevice::ReadWrite)) {
        qDebug() << "error when open json: " << JsonFile;
        return;
    }

    file.resize(0); // 清空文件中的原有内容
    QJsonObject root;//根元素
    QJsonObject metaobj;//元数据
    metaobj.insert("num", filelist.size());
    metaobj.insert("current", 0);
    root.insert("metadata", metaobj);

    QJsonArray imgArray;
    for (int i = 0; i < filelist.size(); i++) {
        QJsonObject imgObject;
        imgObject.insert("id", QString::number(i));
        imgObject.insert("name", filelist[i]);
        imgObject.insert("mask", "");
        imgObject.insert("label", "");
        imgArray.append(imgObject);
    }
    root.insert("imagelist", imgArray);

    QJsonDocument rootDoc;
    rootDoc.setObject(root);

    file.write(rootDoc.toJson());
    file.close();
}
/*
 * private:向json中记录当前文件已经处理完
 */
void MaskGeneratorView::SaveCurrent(QString maskfilename, QString label) {
    QFile file(JsonFile);
    if (!file.open(QIODevice::ReadWrite)) {
        qDebug() << "error when open json: " << JsonFile;
        return;
    }
    QByteArray allData = file.readAll();

    QJsonDocument jdoc(QJsonDocument::fromJson(allData));
    QJsonObject RootObject = jdoc.object();

    QJsonValueRef ImageListJsonRef = RootObject.find("imagelist").value();
    QJsonArray ImageListJson = ImageListJsonRef.toArray();

    QJsonValueRef MetaDateRef = RootObject.find("metadata").value();
    QJsonObject MetaDataObj = MetaDateRef.toObject();
    int current = MetaDataObj["current"].toInt();
    int total = MetaDataObj["num"].toInt();

    //修改list中的下标为current的项
    QJsonArray::iterator ArrayIterator = ImageListJson.begin();
    QJsonValueRef targetValueRef = ArrayIterator[current];
    QJsonObject targetObject = targetValueRef.toObject();
    targetObject["mask"] = maskfilename;
    targetObject["label"] = label;
    targetValueRef = targetObject;
    ImageListJsonRef = ImageListJson;

    //修改current的值
    // if (current == total-1) MetaDataObj["current"] = current;
    // else MetaDataObj["current"]=current+1;
    // MetaDataObj["current"]=current+1;
    MetaDateRef = MetaDataObj;

    //写回到文件
    file.resize(0);
    file.write(QJsonDocument(RootObject).toJson());
    file.close();
}
/*
 * private:获取当前要处理的图片文件名
 */
QString MaskGeneratorView::getCurrentImageName() {
    QFile file(JsonFile);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "error when open json: " << JsonFile;
        return QString();
    }
    QByteArray allData = file.readAll();
    file.close();
    QJsonDocument jdoc(QJsonDocument::fromJson(allData));
    QJsonObject RootObject = jdoc.object();

    QJsonValueRef ImageListJsonRef = RootObject.find("imagelist").value();
    QJsonArray ImageListJson = ImageListJsonRef.toArray();

    QJsonValueRef MetaDateRef = RootObject.find("metadata").value();
    QJsonObject MetaDataObj = MetaDateRef.toObject();
    int current = MetaDataObj["current"].toInt();

    this->current = current;
    this->total = MetaDataObj["num"].toInt();
    QJsonArray::iterator ArrayIterator = ImageListJson.begin();
    QJsonValueRef targetValueRef = ArrayIterator[current];
    QJsonObject targetObject = targetValueRef.toObject();

    return targetObject["name"].toString();
}
/*
 * private:总开关
 */
void MaskGeneratorView::MasterSwitch(bool status) {
    ui.verticalSlider_threshold->setEnabled(status);
    ui.action_show_mask->setEnabled(status);
    ui.action_show_origin->setEnabled(status);
    ui.action_UnDo->setEnabled(status);
    ui.action_Redo->setEnabled(status);
    ui.action_save->setEnabled(status);
    ui.action_prior->setEnabled(status);
    ui.action_next->setEnabled(status);
}
/*
 * private:校验Json文件
 */
bool MaskGeneratorView::CheckJson() {
    QFile file(JsonFile);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "error when open json: " << JsonFile;
        return false;
    }
    try {
        QByteArray allData = file.readAll();
        file.close();
        QJsonDocument jdoc(QJsonDocument::fromJson(allData));
        QJsonObject RootObject = jdoc.object();

        if (!RootObject.contains("imagelist") || !RootObject.contains("metadata")) {
            return false;
        }
    }
    catch (...) {
        return false;
    }

    return true;
}
/*
 * private:加载第一章图片,初始化相关的变量,开始工作
 */
bool MaskGeneratorView::JobStart() {
    //获取当前图片文件名
    QString imgfilename = getCurrentImageName();
    this->target = new cv::Mat(cv::imread((srcpath + "/" + imgfilename).toStdString()));
    cv::cvtColor(*this->target, *this->gray, cv::COLOR_RGB2GRAY);

    std::vector<cv::Mat> channels;
    cv::split(*this->target, channels);
    merge(std::vector<cv::Mat>{channels[2], channels[1], channels[0]}, *this->target);

    if (this->target->empty()) {
        return false;
    }
    //给working和mask赋值
    this->working_img = new cv::Mat(this->target->clone());
    this->mask = new cv::Mat(cv::Mat::zeros(this->target->rows + 2, this->target->cols + 2, CV_8UC1));
    //显示working
    updateUI();
    //历史记录初始化
    HistoryData* initData = new HistoryData(this->threshold, this->iptLasso[nLasso], this->nLasso, this->working_img, this->mask);
//    std::cout << initData << std::endl;
    this->history->clear();
    this->history->add(initData);
    // m_HistoryLogWidget->clear();
    m_HistoryLogWidget->add("Open img");
    //状态条更新
    QString msg;
    msg.sprintf("current=%d,total=%d,filename=%s", this->current + 1, this->total, qPrintable(imgfilename));
    this->m_PosLabel->setText(msg);
    this->m_PosLabel->show();
    return true;
}
/*
 * private:显示图片
 *
 * 这里有内存泄漏!
 */
void MaskGeneratorView::showMat(cv::Mat img) {
    if (!img.empty()) {
        *qimage_to_show = Tools::cvMat2QImage(img);
        if (m_GraphicsScene == nullptr) {
            m_GraphicsScene = new MyQGraphicsScene();
        }
        if (m_GraphicsItem == nullptr) {
            m_GraphicsItem = new MyQGraphicsPixmapItem();
            connect(m_GraphicsItem, SIGNAL(mouseLeftDown(int, int)), this, SLOT(onMouseLeftDown(int, int)));
            connect(m_GraphicsItem, SIGNAL(mouseLeftMoved(int, int)), this, SLOT(onMouseLeftMoved(int, int)));
            connect(m_GraphicsItem, SIGNAL(mouseDoubleClicked(int, int)), this, SLOT(onMouseDoubleClicked(int, int)));
            connect(m_GraphicsItem, SIGNAL(mouseLeftRelease(int, int)), this, SLOT(onMouseLeftRelease(int, int)));
        }
        
        //MaskGeneratorView::setMouseTracking(true);
        //m_GraphicsItem -> setAcceptedMouseButtons(Qt::LeftButton);
        //m_GraphicsItem->setFlag(QGraphicsItem::ItemIsSelectable);//必须加上这句，否则item无法获取到鼠标事件

        m_GraphicsItem->setPixmap(QPixmap::fromImage(*qimage_to_show));
        if (m_GraphicsScene->items().size()<=0||m_GraphicsScene->items()[0] != m_GraphicsItem) {
            m_GraphicsScene->addItem(m_GraphicsItem);
        }
        m_GraphicsView->setScene(m_GraphicsScene);
        m_GraphicsView->show();
    }
}
/*
 * private:刷新工作图片
 */
void MaskGeneratorView::workingImgRefresh() {
    //获取一张原始图像的灰度化版本
    cv::Mat threshold_img;
    //使用当前的threshold进行处理
    cv::threshold(*this->gray, threshold_img, this->threshold, 255, cv::THRESH_TOZERO);
    //    std::vector<std::vector<cv::Point> > contours;
    cv::findContours(threshold_img, this->contours, cv::RETR_LIST, cv::CHAIN_APPROX_NONE);
    //workingimg赋值为原始图加上边缘
    *this->working_img = this->target->clone();//要深拷贝,防止元数据污染
    //把之前画好的mask叠加过来
    cv::Mat _mask = this->mask->clone();
    auto _mask_orgi_size = _mask(cv::Rect(1, 1, this->target->cols, this->target->rows));
    this->working_img->setTo(cv::Scalar(0, 0, 255), _mask_orgi_size);
//    cv::Mat mask_rgb = cv::Mat(this->target->rows, this->target->cols, CV_8UC3, cv::Scalar(0, 0, 0));
//    mask_rgb.setTo(cv::Scalar(0,0,255), _mask_orgi_size);
//    *this->working_img = *this->working_img + mask_rgb*0.8;
    // cv::imshow("mask rgb", mask_rgb);
    drawContours(*this->working_img, contours, -1, cv::Scalar(0, 255, 0), 1);
    updateUI();
}
/*
 * private:更新ui
 */
void MaskGeneratorView::updateUI() {
    switch (this->m_DisplayMode) {
    case ORIGIN:
        showMat(*working_img);
        break;
    case MASK:
        showMat(*mask);
        break;
    }
}
/*
 * private:
 */
void MaskGeneratorView::setCurrentValue() {
    QFile file(JsonFile);
    if (!file.open(QIODevice::ReadWrite)) {
        qDebug() << "error when open json: " << JsonFile;
        return;
    }
    QByteArray allData = file.readAll();

    QJsonDocument jdoc(QJsonDocument::fromJson(allData));
    QJsonObject RootObject = jdoc.object();

    QJsonValueRef ImageListJsonRef = RootObject.find("imagelist").value();
    QJsonArray ImageListJson = ImageListJsonRef.toArray();

    QJsonValueRef MetaDateRef = RootObject.find("metadata").value();
    QJsonObject MetaDataObj = MetaDateRef.toObject();
    int current = MetaDataObj["current"].toInt();
    int total = MetaDataObj["num"].toInt();

    //修改current的值
    switch (this->m_Direction) {
    case PREV:
        // check the low boundary
        if (current == 0) {
            MetaDataObj["current"] = current;
            QMessageBox message(QMessageBox::NoIcon, QStringLiteral("注意!"), QStringLiteral("当前图片已经是列表中第一张图片!"));
            message.exec();
        }
        else
            MetaDataObj["current"] = current - 1;
        break;
    case NEXT:
        // check the high boundary
        if (current == total - 1) {
            MetaDataObj["current"] = current;
            QMessageBox message(QMessageBox::NoIcon, QStringLiteral("注意!"), QStringLiteral("当前图片已经是列表中最后一张图片!请检查是否已经完成标注!"));
            message.exec();
        }
        else
            MetaDataObj["current"] = current + 1;
        break;
    default:
        break;
    }
    MetaDateRef = MetaDataObj;

    //写回到文件
    file.resize(0);
    file.write(QJsonDocument(RootObject).toJson());
    file.close();
}
/*
 * private:保存
 */
void MaskGeneratorView::saveMaskImage() {
    QString savedMaskName = getCurrentImageName().split('.')[0] + "_mask.png";
    QDir dir(srcpath);
    std::string savedMaskPath;
    if (dir.cdUp()) {
        QString savedMaskPrefix = dir.absolutePath() + "/masks/";
        QDir dir(savedMaskPrefix);
        if (!dir.exists()) {
            dir.mkdir(savedMaskPrefix);
        } // if masks dir doesn't exist, then create the mask dir;
        savedMaskPath = (savedMaskPrefix + savedMaskName).toStdString();
        if (savedMaskPath.empty()) {
            QMessageBox message(QMessageBox::NoIcon, "Error!", "Saved Failed: saved image path is empty!");
            message.exec();
            return;
        }
    } // parent dir exists;
    else { return; } // parent dir not exists;
    cv::Mat _mask = this->mask->clone();
    auto _mask_orgi_size = _mask(cv::Rect(1, 1, this->target->cols, this->target->rows));
    if (_mask_orgi_size.empty()) {
        QMessageBox message(QMessageBox::NoIcon, "Error!", "Saved Failed: mask is empty!");
        message.exec();
        return;
    }
    SaveCurrent(savedMaskName, ui.textEdit_label->toPlainText());
    cv::imwrite(savedMaskPath, _mask_orgi_size);
    // this->history->clear();
}


/*
 * Action:打开文件夹
 */
void MaskGeneratorView::onActionTriggered_OpenFolder() {
    /*
     * 打开文件夹,列出文件夹下的所有文件
     * 如果存在列表文件,则读取列表文件,否则创建列表文件
     * 然后显示尚未处理的第一张图片
     */
    //打开文件夹
    QFileDialog * fileDialog = new QFileDialog(this);
    fileDialog->setWindowTitle(QStringLiteral("请选择待标注的图片所在的目录"));
    fileDialog->setFileMode(QFileDialog::Directory);
    if (fileDialog->exec() == QDialog::Accepted) {
        srcpath = fileDialog->selectedFiles()[0];
        //寻找json文件
        JsonFile = srcpath + "/list.json";
        QFileInfo fileInfo(JsonFile);

        if(fileInfo.isFile())//json存在
        {
            //对list进行校验,如果校验通过,则允许使用程序的相关功能
            if(CheckJson()){
                MasterSwitch(true);
                JobStart();
            }else{
                QMessageBox message(QMessageBox::NoIcon, "error!", "Unknown JSON file!");
                message.exec();
            }
        }else{//json不存在
            //先校验当前文件夹
            QDir dir(srcpath);
            QStringList nameFilters;//设置文件过滤器
            nameFilters << "*.jpg"<<"*.png"<<"*.jpeg"<<"*.bmp"<<"*.tif";//将过滤后的文件名称存入到files列表中
            QStringList ImageList = dir.entryList(nameFilters, QDir::Files|QDir::Readable, QDir::Name);
            if(ImageList.size()<=0){
                QMessageBox message(QMessageBox::NoIcon, "error!", "Only \"jpg,png,jpeg,bmp,tif\" files are supported");
                message.exec();
            }else{
                CreateJsonList(ImageList);
                //允许程序的相关功能使用
                MasterSwitch(true);
                JobStart();
            }
        }

    }
}
/*
 * Action:打开文件
 */
void MaskGeneratorView::onActionTriggered_OpenFile() {
    //打开文件
    QFileDialog * fileDialog = new QFileDialog(this);
    fileDialog->setWindowTitle(QStringLiteral("请选择文件列表文件(list.json)"));
    fileDialog->setNameFilter(QStringLiteral("file list(*.json)"));
    fileDialog->setFileMode(QFileDialog::ExistingFile);
    if (fileDialog->exec() == QDialog::Accepted) {
        JsonFile = fileDialog->selectedFiles()[0];
        QFileInfo fileInfo(JsonFile);
        srcpath=fileInfo.absolutePath();
        if(CheckJson()){
            MasterSwitch(true);
            JobStart();
        }else{
            QMessageBox message(QMessageBox::NoIcon, "error!", "Unknown JSON file!");
            message.exec();
        }
    }
}
/*
 * Action:保存
 */
void MaskGeneratorView::onActionTriggered_Save() {
    saveMaskImage();
    this->m_Direction = NEXT;
    setCurrentValue();
    this->history->savedATop();
    m_HistoryLogWidget->clear();
    JobStart();
}
/*
 * Action:退出
 */
void MaskGeneratorView::onActionTriggered_Exit() {

}
/*
 * Action:撤销
 */
void MaskGeneratorView::onActionTriggered_UnDo() {
	HistoryData * history_data=new HistoryData;
    if(this->history->undo(history_data)){
        this->threshold=history_data->threshold;
        this->nLasso = history_data->inumLasso;
        this->iptLasso[nLasso] = history_data->iptLasso;
        ui.verticalSlider_threshold->setValue(this->threshold);

        *this->working_img=*history_data->workingImg;
        *this->mask=*history_data->maskImg;
        showMat(*this->working_img);
        this->m_HistoryLogWidget->undo();
    }
}
/*
 * Action:重做
 */
void MaskGeneratorView::onActionTriggered_ReDo() {
	HistoryData * history_data = new HistoryData;
	if (this->history->redo(history_data)) {
		this->threshold = history_data->threshold;
		this->nLasso = history_data->inumLasso;
		this->iptLasso[nLasso] = history_data->iptLasso;
		ui.verticalSlider_threshold->setValue(this->threshold);

		*this->working_img = *history_data->workingImg;
		*this->mask = *history_data->maskImg;
		showMat(*this->working_img);
		this->m_HistoryLogWidget->redo();
	}
}
/*
 * Action:前一张
 */
void MaskGeneratorView::onActionTriggered_Prior() {
    // if the image has been masked
    if (m_HistoryLogWidget->getCurrentValue() != 0) {
        QMessageBox msgBox;
        msgBox.setText(QStringLiteral("是否保存?"));
        //msgBox.setInformativeText(QStringLiteral("是否保存?"));
        msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Save);
        int ret = msgBox.exec();
        switch (ret) {
            case QMessageBox::Save:
                saveMaskImage();
                break;
//            case QMessageBox::Discard:
//                break;
            case QMessageBox::Cancel:
                return ;
            default:
                break;
        }
        this->m_Direction = PREV;
        setCurrentValue();
        this->history->savedATop();
        m_HistoryLogWidget->clear();
        JobStart();
    }
    else {
        this->m_Direction = PREV;
        setCurrentValue();
        this->history->savedATop();
        m_HistoryLogWidget->clear();
        JobStart();
    }
}
/*
 * Action:下一张
 */
void MaskGeneratorView::onActionTriggered_Next() {
    // if the image has been masked
    if (m_HistoryLogWidget->getCurrentValue() != 0) {
        QMessageBox msgBox;
        msgBox.setText(QStringLiteral("是否保存?"));
        //msgBox.setInformativeText("Do you want to save your changes?");
        msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Save);
        int ret = msgBox.exec();
        switch (ret) {
            case QMessageBox::Save:
                saveMaskImage();
                break;
//            case QMessageBox::Discard:
//                break;
            case QMessageBox::Cancel:
                return ;
            default:
                break;
        }
        this->m_Direction = NEXT;
        setCurrentValue();
        this->history->savedATop();
        m_HistoryLogWidget->clear();
        JobStart();
    }
    else {
        this->m_Direction = NEXT;
        setCurrentValue();
        this->history->savedATop();
        m_HistoryLogWidget->clear();
        JobStart();
    }
}
/*
 * Action:显示原图
 */
void MaskGeneratorView::onActionTriggered_ShowOrigin() {
    ui.action_show_origin->setChecked(true);
    ui.action_show_mask->setChecked(false);
    this->m_DisplayMode=ORIGIN;
    updateUI();
}
/*
 * Action:显示掩码
 */
void MaskGeneratorView::onActionTriggered_ShowMask() {
    ui.action_show_origin->setChecked(false);
    ui.action_show_mask->setChecked(true);
    this->m_DisplayMode=MASK;
    updateUI();
}
/*
 * Action : 查漏
 * 检查文件列表中0-current,是否确实被标记了,且存储了mask
 * check的结果用messagebox提示出来
 * 并确保0-current都是确实被标记完的
 */
void MaskGeneratorView::onActionTriggered_Check() {
    QFile file(JsonFile);
    if (!file.open(QIODevice::ReadWrite)) {
        qDebug() << "error when open json: " << JsonFile;
        return;
    }
    QByteArray allData = file.readAll();

    QJsonDocument jdoc(QJsonDocument::fromJson(allData));
    QJsonObject RootObject = jdoc.object();

    QJsonValueRef ImageListJsonRef = RootObject.find("imagelist").value();
    QJsonArray ImageListJson = ImageListJsonRef.toArray();

    QJsonValueRef MetaDateRef = RootObject.find("metadata").value();
    QJsonObject MetaDataObj = MetaDateRef.toObject();

    int current = MetaDataObj["current"].toInt();
    int total = MetaDataObj["num"].toInt();
    int num = current;//前current个元素中到底有多少是真的被标注完了.
                    //注意current实际上表示的是从前往后第一个没有被标注的元素的下标,从0开始,所以current表示的也是有多少个元素被标注过了

    //读取ImageListJson,判别是否被标记,进行移动,然后写回到ImageListJsonRef
    for (auto iter = ImageListJson.begin(); iter != ImageListJson.end() && current > 0; iter++) {
        //如果'mask字段是空的,或者mask文件夹里面没有对应的文件,那么这个条目就是没有被标注的'
        if (iter['mask'].toString().isEmpty()) {
            QJsonValue temp = *iter.a;
            iter = ImageListJson.erase(iter);
            iter = ImageListJson.insert(ImageListJson.end(), temp);
            num--;
        }
        //无论如何,我们只检查current个元素
        current--;
    }
    //修改current的值
    MetaDataObj["current"] = num;

    //对象写回
    MetaDateRef = MetaDataObj;
    ImageListJsonRef = ImageListJson;
    //写回到文件
    file.resize(0);
    file.write(QJsonDocument(RootObject).toJson());
    file.close();
}
/*
 * Action:设置
 */
void MaskGeneratorView::onActionTriggered_Settings() {

}
/*
 * Action:帮助
 */
void MaskGeneratorView::onActionTriggered_Help() {
    std::cout << "help" << std::endl;
}
/*
 * Action:关于
 */
void MaskGeneratorView::onActionTriggered_About() {
    QMessageBox message(QMessageBox::NoIcon, QStringLiteral("眼底图像标注器"), QStringLiteral("吉林大学图形学与数字媒体实验室"));
    message.exec();
}
/*
 * Event:响应阈值滑动条的运动
 */
void MaskGeneratorView::onValueChanged_threshold(int value) {
    if(this->threshold!=value){
        this->threshold = value;
        workingImgRefresh();
    }
}
/*
 * Event:响应铅笔粗细滑动条的运动
 */
void MaskGeneratorView::onValueChanged_thickness(int value) {
    this->m_CursorType = PEN;
    if(this->thickness!=value){
        this->thickness = value;
    }
}
/*
 * Event:响应graphics区域的鼠标滚轮动作:缩放图片
 */
void MaskGeneratorView::onMouseWheelZoom(int delta) {
    //qDebug() << QString::fromStdString(std::to_string(delta));
    if (delta > 0 && scaleFactor >= 0)
    {
        scaleFactor += 5;
        QMatrix old_matrix;
        old_matrix = m_GraphicsView->matrix();
        m_GraphicsView->resetMatrix();
        m_GraphicsView->translate(old_matrix.dx(), old_matrix.dy());
        m_GraphicsView->scale(scaleFactor / 100.0, scaleFactor / 100.0);
    }
    else if (delta < 0 && scaleFactor >= 0)
    {
        scaleFactor -= 5;
        QMatrix old_matrix;
        old_matrix = m_GraphicsView->matrix();
        m_GraphicsView->resetMatrix();
        m_GraphicsView->translate(old_matrix.dx(), old_matrix.dy());
        m_GraphicsView->scale(scaleFactor / 100.0, scaleFactor / 100.0);
    }
    else if (scaleFactor < 0) {
        scaleFactor = 0.0;
    }
}
/*
 * Event:响应绘图区域的鼠标双击事件
 */
void MaskGeneratorView::onMouseDoubleClicked(int x, int y) {
    //qDebug("双击事件");
    if (this->m_CursorType == POLY_LASSO) {
        const cv::Point * ppt[1] = {lassoPoints[nLasso]};
        int npt[] = { iptLasso[nLasso] };
        cv::polylines(*this->working_img, ppt, npt, 1, 1, cv::Scalar(0, 0, 255));
        cv::fillPoly(*this->working_img, ppt, npt, 1, cv::Scalar(0, 0, 255 ));
        cv::polylines(*this->mask, ppt, npt, 1, 1, cv::Scalar(255, 255, 255));
        cv::fillPoly(*this->mask, ppt, npt, 1, cv::Scalar(255, 255, 255));
        nLasso++;
    }
    updateUI();
    HistoryData* history_data=new HistoryData(this->threshold, this->iptLasso[nLasso], this->nLasso, this->working_img,this->mask);
    this->history->add(history_data);
    QString msg;
    msg.sprintf("Mark at (%d,%d)", x,y);
    m_HistoryLogWidget->add(msg);
}
/*
 * Event:响应绘图区域的左键按下事件
 */
void MaskGeneratorView::onMouseLeftDown(int x, int y)
{
    if(this->m_DisplayMode==ORIGIN){
        switch (this->m_CursorType)//MOUSE,PEN, MAGIC_WAND, FILL_COLOR,POLY_LASSO
        {
        case MOUSE://鼠标指针
        {
            break;
        }
        case PEN://铅笔:按下画图,拖动画图,抬起记录历史
        {
            cv::circle(*this->working_img, cv::Point(x, y), this->thickness, cv::Scalar(0, 0, 255), -1);
            cv::circle(*this->mask, cv::Point(x, y), this->thickness, cv::Scalar(255, 0, 0), -1);
            break;
        }
        case MAGIC_WAND://魔棒:按下操作,抬起记录历史
        {
            std::cout << "magic wand" << std::endl;
            cv::Point seed = cv::Point(x, y);   // get the mouse clicked pos;
            cv::Scalar fill_color = cv::Scalar(0, 0, 255);
            cv::Rect ccomp;
            int flags = 8 | cv::FLOODFILL_FIXED_RANGE | (255 << 8);  //四联通 | ??  | 填充颜色
            // int flags = 8 | 0 | (255 << 8);  //四联通 | ??  | 填充颜色
            cv::floodFill(*this->working_img, *this->mask, seed, fill_color, &ccomp, cv::Scalar(10,10,7), cv::Scalar(10,10,7), flags);
            break;
        }
        case FILL_COLOR://填充:按下操作,抬起记录历史
        {
            cv::Point seed = cv::Point(x, y);   // get the mouse clicked pos;
            cv::Scalar fill_color = cv::Scalar(0, 0, 255);
//            cv::Rect ccomp;
//            int flags = 4 | cv::FLOODFILL_FIXED_RANGE | (255 << 8);  //四联通 | ??  | 填充颜色
            //working_img 和 mask都会被改
            //floodFill(*this->working_img,*this->mask,
            //          seed, fill_color, &ccomp, cv::Scalar(20, 20, 5), cv::Scalar(20, 20, 5), flags);
            //std::cout << this->contours.size() << endl;
            //std::cout << cv::pointPolygonTest(this->contours[0], seed, false) << std::endl;
            int ic = 0;
            for (; ic < this->contours.size(); ic++) {
                int i = cv::pointPolygonTest(this->contours[ic], seed, false);
                if (i != -1) {
                    cv::drawContours(*this->working_img, this->contours, ic, fill_color, cv::FILLED);
                    // apply to mask image;
                    cv::drawContours(*this->mask, this->contours, ic, cv::Scalar(255,255,255), cv::FILLED);
                    break;
                }
            }
            break;
        }
        case POLY_LASSO://多边形套索
        {
            cv::Point prePoint;
            cv::Point curPoint;
            if (iptLasso[nLasso] == 0) {
                prePoint = cv::Point(x, y);
                curPoint = cv::Point(x, y);
            }
            else {
                prePoint = this->lassoPoints[nLasso][iptLasso[nLasso] - 1];
                curPoint = cv::Point(x, y);
            }
            this->lassoPoints[nLasso][iptLasso[nLasso]] = curPoint;
            iptLasso[nLasso]++;
            const cv::Point* ppt[1] = { lassoPoints[nLasso] };
            int npt[] = { iptLasso[nLasso] };
            cv::polylines(*this->working_img, ppt, npt, 1, 0, cv::Scalar(255, 0, 0));
            break;
        }
        default:
            break;
        }
        updateUI();
    }
}
/*
 * Event:响应绘图区域的鼠标左键拖动事件
 */
void MaskGeneratorView::onMouseLeftMoved(int x, int y) {
    if (this->m_CursorType == PEN) {
        cv::circle(*this->working_img, cv::Point(x, y), this->thickness, cv::Scalar(0, 0, 255), -1);
        cv::circle(*this->mask, cv::Point(x, y), this->thickness, cv::Scalar(255, 255, 255), -1);
        updateUI();
    }
}
/*
 * Event:响应绘图区域的左键抬起事件
 */
void MaskGeneratorView::onMouseLeftRelease(int x, int y)
{
    // std::cout << "release" << std::endl;
    if (this->m_CursorType == PEN||
        this->m_CursorType == MAGIC_WAND|| this->m_CursorType == FILL_COLOR|| this->m_CursorType == POLY_LASSO) 
    {
        HistoryData* history_data = new HistoryData(this->threshold, this->iptLasso[nLasso], this->nLasso, this->working_img, this->mask);
        //onTest();
        // std::cout << history_data << std::endl;
        this->history->add(history_data);
        //onTest();
        updateUI();
    }
    QString msg;
    switch (this->m_CursorType)
    {
    case MOUSE:
        break;
    case PEN:
        msg.sprintf("Draw in (%d,%d)", x, y);
        m_HistoryLogWidget->add(msg);
        break;
    case MAGIC_WAND:
        msg.sprintf("Magic Wand in (%d,%d)", x, y);
        m_HistoryLogWidget->add(msg);
        break;
    case FILL_COLOR:
        msg.sprintf("Fill in (%d,%d)", x, y);
        m_HistoryLogWidget->add(msg);
        break;
    case POLY_LASSO:
        msg.sprintf("Poly point in (%d,%d)", x, y);
        m_HistoryLogWidget->add(msg);
        break;
    default:
        break;
    }
}
/*
 * Event:响应窗口关闭事件
 */
void MaskGeneratorView::closeEvent(QCloseEvent *event) {
    // if don't open the image, close directly
    if (m_HistoryLogWidget->getCurrentValue() == -1) { return ; }

    // if the image has been masked
    if (m_HistoryLogWidget->getCurrentValue() != 0) {
        QMessageBox msgBox;
        msgBox.setText("The image has been masked.");
        msgBox.setInformativeText("Do you want to save your changes?");
        msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Save);
        msgBox.setWindowFlags(Qt::WindowCloseButtonHint | Qt::MSWindowsFixedSizeDialogHint);
        int ret = msgBox.exec();
        switch (ret) {
            case QMessageBox::Save:
                saveMaskImage();
                break;
            case QMessageBox::Cancel:
                return ;
            default:
                break;
        }
        return ;
    }
}
/*
 * 按钮:画笔->鼠标指针
 */
void MaskGeneratorView::onPushButtonDown_Mouse(){
    ui.pushButton_mouse->setChecked(true);
    if (this->m_CursorType != MOUSE)
    {
        this->m_CursorType = MOUSE;
        ui.pushButton_pen->setChecked(false);
        ui.pushButton_Magic->setChecked(false);
        ui.pushButton_Fill->setChecked(false);
        ui.pushButton_Lasso->setChecked(false);
    }
}
/*
 * 按钮:画笔->铅笔
 */
void MaskGeneratorView::onPushButtonDown_Pen() {
    ui.pushButton_pen->setChecked(true);
    if (this->m_CursorType != PEN)
    {
        this->m_CursorType = PEN;
        ui.pushButton_mouse->setChecked(false);
        ui.pushButton_Magic->setChecked(false);
        ui.pushButton_Fill->setChecked(false);
        ui.pushButton_Lasso->setChecked(false);
    }
}
/*
 * 按钮:画笔->魔棒工具
 */
void MaskGeneratorView::onPushButtonDown_Magic()
{   
    ui.pushButton_Magic->setChecked(true);
    if (this->m_CursorType != MAGIC_WAND)
    {
        this->m_CursorType = MAGIC_WAND;
        ui.pushButton_mouse->setChecked(false);
        ui.pushButton_pen->setChecked(false);
        ui.pushButton_Fill->setChecked(false);
        ui.pushButton_Lasso->setChecked(false);
    }
}
/*
 * 按钮:画笔->填充工具
 */
void MaskGeneratorView::onPushButtonDown_Fill()
{
    ui.pushButton_Fill->setChecked(true);
    if (this->m_CursorType != FILL_COLOR)
    {
        this->m_CursorType = FILL_COLOR;
        ui.pushButton_mouse->setChecked(false);
        ui.pushButton_pen->setChecked(false);
        ui.pushButton_Magic->setChecked(false);
        ui.pushButton_Lasso->setChecked(false);
    }
}
/*
 * 按钮:画笔->多边形套索
 */
void MaskGeneratorView::onPushButtonDown_Lasso()
{
    ui.pushButton_Lasso->setChecked(true);
    if (this->m_CursorType != POLY_LASSO)
    {
        this->m_CursorType = POLY_LASSO;
        ui.pushButton_mouse->setChecked(false);
        ui.pushButton_pen->setChecked(false);
        ui.pushButton_Magic->setChecked(false);
        ui.pushButton_Fill->setChecked(false);
    }
}
/*
 * 测试栈
 */
void MaskGeneratorView::onTest()
{
//    cv::Mat img = cv::imread("/home/tlss/repos/vrlab/MaskGenerator/Example/images/21_training.tif");
//
//    cv::Mat pure_blue = cv::Mat(img.rows, img.cols, CV_8UC3, cv::Scalar(255, 0, 0));
//    cv::Mat res = img + pure_blue;
//    cv::imshow("test", res);
    /*
    std::cout << "on test button" << std::endl;
    int i = 0;
    while (!history->stackA->empty())
    {
        cv::imshow("A"+std::to_string(i),*history->stackA->top()->workingImg);
        std::cout << history->stackA->top()->workingImg << std::endl;
        history->stackA->pop();
        i++;
    }
    i = 0;
    while (!history->stackB->empty())
    {
        cv::imshow("B" + std::to_string(i), *history->stackB->top()->workingImg);
        // std::cout << history->stackB->top()->workingImg;
        history->stackB->pop();
        i++;
    }
    */
}
