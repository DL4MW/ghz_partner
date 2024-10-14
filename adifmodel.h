#ifndef ADIFMODEL_H
#define ADIFMODEL_H


#include <QtCore>

// Error codes
#define ERR_NO   0
#define ERR_FILE_OPEN  -1

typedef void (*ProcessCallBack)(int);

struct AdifOptions{
        int lineLen;

        AdifOptions();
};

struct AdifRecord{
        bool selected;
        int qsoNr;
        QStringList field;
   AdifRecord(){selected=false;}
   void SetEmpty(int fields);
   int SetField(int index, QString data);
};

struct AdifField{
        QString adif,name,description;
};

struct SelectionCriteria{
        QRegExp regExp;
        int dataIdx;
};


class AdifDataBase:public QObject{
        Q_OBJECT
        protected:
        QList<AdifField> fields;
   QList<AdifRecord> qso_s;
   QList<SelectionCriteria> selectionList;
   QDateTime to,from;
   bool useSelection,sendSignal,wasChanged,selectionDateValid;
   int lastQSO;
        void ResetSelectionCriteria(){selectionDateValid=false;selectionList.clear();}
        public:
        AdifDataBase();
   void Init(QSettings *settings, QString adifTableName);
        int AddQSO(AdifRecord rec);
        void NewDatabase();
        void SetQSOData(int index, int dataIdx, QString data);
        void DeleteQSO(int index);
        void Select(int dataIdx,QString pattern, bool subString,bool isOr);
        void Unselect(int dataIdx,QString pattern, bool subString);
        void Select(QDateTime from, QDateTime to, bool isOr);
        void Unselect(QDateTime from, QDateTime to);
   void Unselect(int qsoNr);
        void AddSelection(int dataIdx,QString pattern, bool subString);
        void AddSelection(QDateTime from, QDateTime to);
   void Select();
   void Unselect();

   void ClearSelection();
        bool IsSelected(int index);
        bool SelectionStatus(){return useSelection;}
        int GetQSOCount();
        int GetSelectedCount();
        int GetFieldCount();
        int DataIdx(QString adifField);
        QString AdifName(int dataIdx);
        QString FieldName(int dataIdx);
        QString FieldDescription(int dataIdx);
        int AddField(QString adifName,QString fieldName,QString description);
        AdifRecord GetQSO(int index);
        int GetIndex(int qsoNo);
        int GetSelectedIndex(int index);
        bool SetSendSignal(bool on);
        void PopulateChanges(){emit AdifChanged();emit DataChanged();emit SelectionChanged();}
        int ExportAdif(QString fileName,AdifOptions options, bool selected, ProcessCallBack callback);
        int ImportAdif(QString fileName,AdifOptions options, ProcessCallBack callback);
        bool Changed(){return wasChanged;}
        void ResetChanged(){wasChanged=false;}
        signals:
        void DataChanged();
        void AdifChanged();
        void SelectionChanged();
};



#endif // ADIFMODEL_H
