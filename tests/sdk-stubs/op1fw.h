#ifndef SCREENMIRROR_HOST_OP1FW_H
#define SCREENMIRROR_HOST_OP1FW_H
/* Test-only firmware-call declarations; implementations are host mocks. */
int fw_usb_configured(void);
void fw_midi_send(int cable,const unsigned int *events,int count);
#endif
