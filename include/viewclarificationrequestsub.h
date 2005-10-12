#ifndef __VIEW_CLARIFICATION_REQUEST_SUB__
#define __VIEW_CLARIFICATION_REQUEST_SUB__

#include "ui_viewclarificationrequest.h"
#include "serverconnection.h"

class ViewClarificationRequestSub : public ViewClarificationRequest {
private:
        uint32_t _clarification_request_id;
        ServerConnection *_server_con;

protected:
        virtual void doReply();

public:
	ViewClarificationRequestSub(uint32_t id, ServerConnection *server_con);
        virtual ~ViewClarificationRequestSub();
};

#endif
