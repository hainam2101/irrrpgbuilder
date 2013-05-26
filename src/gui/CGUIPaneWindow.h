#ifndef INC_CGUIPANEWINDOW_H
#define INC_CGUIPANEWINDOW_H

#include <irrlicht.h>

using namespace irr;
using namespace gui;

/** Class for have pane windows
	Will be used for most of the application windows, as sub-editor will require this
	
	Note for the class will need to access the Irrlicht device (cursor pos)
	This is done using the "setDevice()" command
	*/


class CGUIPaneWindow : public IGUIElement {
      
   public:
       
        /** 
      \brief Constructor
      \param title - The title of the dialog
      \param environment - The GUI environment to be used
      \param parent - The parent of the dialog
      \param id - The ID of the dialog
      \param type - The type of dialog
      */
      CGUIPaneWindow(const wchar_t* title, IGUIEnvironment* environment, IGUIElement* parent, s32 id, core::rect<s32> rectangle);
		 // : IGUIElement(EGUIET_WINDOW, environment, parent, id, rectangle) {}

      /**
        \brief Destructor
        */
      virtual ~CGUIPaneWindow();
      
      /**
        \brief Returns the filename of the selected file. Returns NULL, if no file was selected.
        \return a const wchar_t*
        */
      
	  //! Returns pointer to the close button
	  IGUIButton* getCloseButton() const;
	
      virtual bool OnEvent(const SEvent& event);

      /**
      \brief Render function
      */
      virtual void draw();  

	  virtual inline void setDraggable (bool drag) {IsDraggable=drag;}
	  virtual inline void setDrawTitlebar (bool draw) {drawTitleBar=draw;}

	  // Device 
	  inline void setDevice(irr::IrrlichtDevice * device) { this->device=device;}

	  core::rect<s32> getClientRect();

	  bool enablebottom, enabletop, enableright, enableleft;

	  void expandLeft();
	  void retractLeft();

	  bool leftStatus();

   protected:
	   
	   core::position2d<s32> DragStart;
	   core::rect<s32> ClientRect;
	   
	   IGUIElement* EventParent;IGUIButton* CloseButton;

	  private:

		// mwethods
		void drawRef(core::vector2d<s32> mousepos);
		bool drawStretch(core::vector2d<s32> mousepos);

		// variables
		bool stretchbottom, stretchtop, stretchright, stretchleft;
		irr::IrrlichtDevice * device;

		IGUIScrollBar * scroll;
		s32 scrollpos;

		bool Dragging, IsDraggable, drawTitleBar;
        bool DrawBackground;
        bool DrawTitlebar;
		bool IsActive;
		bool DrawInsideBorder;
		bool DragByTitlebar;
		u32 borderwidth;

		//contain the original position of the rectangle.
		core::rect<s32> oldrectangle;

		u32 timer1;
		u32 timer2;

		bool expand, retract;
		  
         
};

#endif /* CGUIPANEWINDOW_H */