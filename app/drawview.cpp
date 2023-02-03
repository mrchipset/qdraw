#include "drawview.h"
#include "drawscene.h"
#include <QSvgGenerator>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QOpenGLWidget>

//http://www.w3.org/TR/SVG/Overview.html

DrawView::DrawView(QGraphicsScene *scene)
    :QGraphicsView(scene),
      mPrimaryImage(nullptr)
{
    m_hruler = new QtRuleBar(Qt::Horizontal,this,this);
    m_hruler->setLabel(tr("X Axis"));
    m_vruler = new QtRuleBar(Qt::Vertical,this,this);
    m_vruler->setLabel(tr("Y Axis"));
    box = new QtCornerBox(this);
    setViewport(new QWidget);

    setAttribute(Qt::WA_DeleteOnClose);
    isUntitled = true;

    modified = false;
    setCacheMode(QGraphicsView::CacheNone);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
//    setViewport(new QOpenGLWidget());
}

void DrawView::zoomIn()
{
    scale(1.2,1.2);
    updateRuler();
}

void DrawView::zoomOut()
{
    if (transform().m11() / 1.2 < contentsRect().width() / scene()->width() ) {
//        const double _scale = (contentsRect().width() / scene()->width());
//        resetTransform();
//        scale(_scale, _scale);
        if (mPrimaryImage) {
            fitInView(scene()->sceneRect());
            centerOn(mPrimaryImage);
        }
    } else {
        scale(1 / 1.2, 1 / 1.2);
    }
    updateRuler();
}

void DrawView::newFile()
{
    static int sequenceNumber = 1;

    isUntitled = true;
    curFile = tr("drawing%1.xml").arg(sequenceNumber++);
    setWindowTitle(curFile + "[*]");
}

bool DrawView::loadFile(const QString &fileName)
{
    QImageReader reader(fileName);
    if (reader.imageFormat() == QImage::Format_Invalid) {
        QFile file(fileName);
        if (!file.open(QFile::ReadOnly | QFile::Text)) {
            QMessageBox::warning(this, tr("Qt Drawing"),
                tr("Cannot read file %1:\n%2.")
                .arg(fileName)
                .arg(file.errorString()));
            return false;
        }

        QXmlStreamReader xml(&file);
        if (xml.readNextStartElement()) {
            if (xml.name() == tr("canvas"))
            {
                int width = xml.attributes().value(tr("width")).toInt();
                int height = xml.attributes().value(tr("height")).toInt();
                scene()->setSceneRect(0, 0, width, height);
                loadCanvas(&xml);
            }
        }

        setCurrentFile(fileName);
        qDebug() << xml.errorString();
        return !xml.error();
    }
    else {
        QImage img;
        if (!reader.read(&img)) {
            return false;
        }
        else {
            QGraphicsPixmapItem* pItem = new QGraphicsPixmapItem();
            pItem->setPixmap(QPixmap::fromImage(img));
            setPrimaryImage(pItem);
        }
    }

    return true;
}

bool DrawView::save()
{
    if (isUntitled) {
        return saveAs();
    } else {
        return saveFile(curFile);
    }
}

bool DrawView::saveAs()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save As"),
                                                    curFile);
    if (fileName.isEmpty())
        return false;

    return saveFile(fileName);

}

bool DrawView::saveFile(const QString &fileName)
{

    QFile file(fileName);
    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        QMessageBox::warning(this, tr("Qt Drawing"),
                             tr("Cannot write file %1:\n%2.")
                             .arg(fileName)
                             .arg(file.errorString()));
        return false;
    }

    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeDTD("<!DOCTYPE qdraw>");
    xml.writeStartElement("canvas");
    xml.writeAttribute("width",QString("%1").arg(scene()->width()));
    xml.writeAttribute("height",QString("%1").arg(scene()->height()));

    foreach (QGraphicsItem *item , scene()->items()) {
        AbstractShape * ab = qgraphicsitem_cast<AbstractShape*>(item);
        QGraphicsItemGroup *g = dynamic_cast<QGraphicsItemGroup*>(item->parentItem());
        if ( ab &&!qgraphicsitem_cast<SizeHandleRect*>(ab) && !g ){
            ab->saveToXml(&xml);
        }
    }
    xml.writeEndElement();
    xml.writeEndDocument();
#if 0
    QSvgGenerator generator;
    generator.setFileName(fileName);
    generator.setSize(QSize(800, 600));
    generator.setTitle(tr("SVG Generator Example Drawing"));
    generator.setDescription(tr("An SVG drawing created by the SVG Generator "
                                "Example provided with Qt."));
//![configure SVG generator]
//![begin painting]
    QPainter painter;
    painter.begin(&generator);
//![begin painting]
//!
    scene()->clearSelection();
    scene()->render(&painter);
//![end painting]
    painter.end();
//![end painting]
#endif
    setCurrentFile(fileName);
    return true;
}

QString DrawView::userFriendlyCurrentFile()
{
    return strippedName(curFile);
}

bool DrawView::setPrimaryImage(QGraphicsPixmapItem* image)
{
    // 1. set the scene
    // 2. add the image to scene
    // 3. fit the view
    if (mPrimaryImage) {
        scene()->removeItem(mPrimaryImage);
    }
    mPrimaryImage = image;
    QRectF rect = mPrimaryImage->boundingRect();
    rect.translate(QPointF(-rect.width(), -rect.height()));
    mPrimaryImage->setPos(QPointF(-rect.width(), -rect.height()));
    scene()->setSceneRect(rect);
    scene()->addItem(mPrimaryImage);
    // fit to the view
    fitInView(mPrimaryImage, Qt::IgnoreAspectRatio);
    centerOn(mPrimaryImage);
    updateRuler();
    qDebug() << transform().m11() << viewport()->contentsRect().width() / scene()->width();
    return true;
}

QGraphicsPixmapItem* DrawView::getPrimaryImage() const
{
    return mPrimaryImage;
}

bool DrawView::appendImage(QGraphicsPixmapItem* image)
{
    // add image to list
    // add image to scene
    if (mImages.contains(image)) {
        return false;
    } else {
        mImages.append(image);
        scene()->addItem(image);
        return true;
    }
}

bool DrawView::removeImage(QGraphicsPixmapItem* image)
{
    if (mImages.contains(image)) {
        mImages.removeAll(image);
        scene()->removeItem(image);
        return true;
    } else {
        return false;
    }
}

void DrawView::closeEvent(QCloseEvent *event)
{
    if (maybeSave()) {
        event->accept();
    } else {
        event->ignore();
    }
}

void DrawView::wheelEvent(QWheelEvent *event)
{
    const int xAngle = event->angleDelta().x() / 8;
    const int yAngle = event->angleDelta().y() / 8;
    if (yAngle != 0) {
        if (yAngle > 0) {
            zoomIn();
        } else {
            zoomOut();
        }
    } else {
        if (xAngle!=0) {
            if (xAngle > 0) {
                zoomIn();
            } else {
                zoomOut();
            }
        }
    }
    event->accept();
}

void DrawView::mouseMoveEvent(QMouseEvent *event)
{
    QPointF pt =mapToScene(event->pos());
    m_hruler->updatePosition(event->pos());
    m_vruler->updatePosition(event->pos());
    emit positionChanged( pt.x() , pt.y() );
    QGraphicsView::mouseMoveEvent(event);
}

void DrawView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);

    this->setViewportMargins(m_vruler->getRulerSize()-1,m_hruler->getRulerSize()-1,0,0);
    m_hruler->resize(this->size().width()- m_vruler->getRulerSize() - 1,m_hruler->getRulerSize());
    m_hruler->move(m_hruler->getRulerSize(),0);
    m_vruler->resize(m_vruler->getRulerSize(),this->size().height() - m_vruler->getRulerSize() - 1);
    m_vruler->move(0,m_hruler->getRulerSize());

    box->resize(m_vruler->getRulerSize(),m_hruler->getRulerSize());
    box->move(0,0);

    // set the scale factor to 1.0
    if (mPrimaryImage) {
        fitInView(mPrimaryImage, Qt::IgnoreAspectRatio);
        centerOn(mPrimaryImage);
    }

    updateRuler();
}

void DrawView::scrollContentsBy(int dx, int dy)
{
    QGraphicsView::scrollContentsBy(dx,dy);
    updateRuler();
}

void DrawView::updateRuler()
{
    if ( scene() == 0) return;
    QRectF viewbox = this->rect();
    QPointF offset = mapFromScene(scene()->sceneRect().topLeft());
    double factor =  1./transform().m11();
    double lower_x = factor * ( viewbox.left()  - offset.x() );
    double upper_x = factor * ( viewbox.right() -RULER_SIZE- offset.x()  );
    m_hruler->setRange(lower_x,upper_x,upper_x - lower_x );
    m_hruler->update();

    double lower_y = factor * ( viewbox.top() - offset.y()) * -1;
    double upper_y = factor * ( viewbox.bottom() - RULER_SIZE - offset.y() ) * -1;

    m_vruler->setRange(lower_y,upper_y,upper_y - lower_y );
    m_vruler->update();

    //qDebug()<<viewbox<<QPoint(lower_x,upper_x) << QPoint(lower_y,upper_y) << offset;
}

bool DrawView::event(QEvent* ev)
{
    bool retVal = QGraphicsView::event(ev);
    if (ev->type() == QEvent::Polish) {
        // call fitView
        QTimer::singleShot(200, this, &DrawView::showAll);
    }
    return retVal;
}

bool DrawView::maybeSave()
{
    if (isModified()) {
        QMessageBox::StandardButton ret;
        ret = QMessageBox::warning(this, tr("MDI"),
                     tr("'%1' has been modified.\n"
                        "Do you want to save your changes?")
                     .arg(userFriendlyCurrentFile()),
                     QMessageBox::Save | QMessageBox::Discard
                     | QMessageBox::Cancel);
        if (ret == QMessageBox::Save)
            return save();
        else if (ret == QMessageBox::Cancel)
            return false;
    }
    return true;
}

void DrawView::setCurrentFile(const QString &fileName)
{
    curFile = QFileInfo(fileName).canonicalFilePath();
    isUntitled = false;
    setModified(false);
    setWindowModified(false);
    setWindowTitle(userFriendlyCurrentFile() + "[*]");
}

QString DrawView::strippedName(const QString &fullFileName)
{
    return QFileInfo(fullFileName).fileName();

}

void DrawView::loadCanvas( QXmlStreamReader *xml)
{
    Q_ASSERT(xml->isStartElement() && xml->name() == "canvas");

    while (xml->readNextStartElement()) {
        AbstractShape * item = NULL;
        if (xml->name() == tr("rect")){
            item = new GraphicsRectItem(QRect(0,0,1,1));
        }else if (xml->name() == tr("roundrect")){
            item = new GraphicsRectItem(QRect(0,0,1,1),true);
        }else if (xml->name() == tr("ellipse"))
            item = new GraphicsEllipseItem(QRect(0,0,1,1));
        else if (xml->name()==tr("polygon"))
            item = new GraphicsPolygonItem();
        else if ( xml->name()==tr("bezier"))
            item = new GraphicsBezier();
        else if ( xml->name() == tr("polyline"))
            item = new GraphicsBezier(false);
        else if ( xml->name() == tr("line"))
            item = new GraphicsLineItem();
        else if ( xml->name() == tr("group"))
            item =qgraphicsitem_cast<AbstractShape*>(loadGroupFromXML(xml));
        else
            xml->skipCurrentElement();

        if (item && item->loadFromXml(xml))
            scene()->addItem(item);
        else if ( item )
            delete item;
    }
}

GraphicsItemGroup *DrawView::loadGroupFromXML(QXmlStreamReader *xml)
{
    QList<QGraphicsItem*> items;
    qreal angle = xml->attributes().value(tr("rotate")).toDouble();
    while (xml->readNextStartElement()) {
        AbstractShape * item = NULL;
        if (xml->name() == tr("rect")){
            item = new GraphicsRectItem(QRect(0,0,1,1));
        }else if (xml->name() == tr("roundrect")){
            item = new GraphicsRectItem(QRect(0,0,1,1),true);
        }else if (xml->name() == tr("ellipse"))
            item = new GraphicsEllipseItem(QRect(0,0,1,1));
        else if (xml->name()==tr("polygon"))
            item = new GraphicsPolygonItem();
        else if ( xml->name()==tr("bezier"))
            item = new GraphicsBezier();
        else if ( xml->name() == tr("polyline"))
            item = new GraphicsBezier(false);
        else if ( xml->name() == tr("line"))
            item = new GraphicsLineItem();
        else if ( xml->name() == tr("group"))
            item =qgraphicsitem_cast<AbstractShape*>(loadGroupFromXML(xml));
        else
            xml->skipCurrentElement();
        if (item && item->loadFromXml(xml)){
            scene()->addItem(item);
            items.append(item);
        }else if ( item )
            delete item;
    }

    if ( items.count() > 0 ){
        DrawScene * s = dynamic_cast<DrawScene*>(scene());
        GraphicsItemGroup * group = s->createGroup(items,false);
        if (group){
            group->setRotation(angle);
            group->updateCoordinate();
            //qDebug()<<"angle:" <<angle;
        }
        return group;
    }
    return 0;
}

void DrawView::showAll()
{
    fitInView(scene()->sceneRect(), Qt::IgnoreAspectRatio);
    updateRuler();
}



