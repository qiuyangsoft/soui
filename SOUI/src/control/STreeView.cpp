#include "souistd.h"
#include "control/STreeView.h"


namespace SOUI 
{
	class STreeViewDataSetObserver : public TObjRefImpl<ITvDataSetObserver>
	{
	public:
		STreeViewDataSetObserver(STreeView *pView):m_pOwner(pView)
		{
		}
		virtual void onChanged(HTREEITEM hBranch);
		virtual void onInvalidated(HTREEITEM hBranch);

	protected:
		STreeView * m_pOwner;
	};


	void STreeViewDataSetObserver::onChanged(HTREEITEM hBranch)
	{
		m_pOwner->onDataSetChanged(hBranch);
	}

	void STreeViewDataSetObserver::onInvalidated(HTREEITEM hBranch)
	{
		m_pOwner->onDataSetInvalidated(hBranch);
	}
	
	//////////////////////////////////////////////////////////////////////////
    class STreeViewItemLocator : public TObjRefImpl<ITreeViewItemLocator>
    {
    public:
        STreeViewItemLocator():m_nLineHeight(50),m_nIndent(10),m_szDef(10,50)
        {

        }

        ~STreeViewItemLocator()
        {

        }

        virtual void SetAdapter(ITvAdapter *pAdapter)
        {
            m_adapter = pAdapter;
        }

        virtual void OnDataSetChanged(HTREEITEM hItem)
        {//初始化列表项高度等数据
            _InitTreeView(hItem);
        }

        virtual int GetTotalHeight() const
        {
            return (int)m_adapter->GetItemDataByIndex(ITvAdapter::ITEM_ROOT,ITvAdapter::DATA_INDEX_BRANCH_HEIGHT);
        }

        virtual int GetTotalWidth() const
        {
            return (int)m_adapter->GetItemDataByIndex(ITvAdapter::ITEM_ROOT,ITvAdapter::DATA_INDEX_BRANCH_WIDTH);
        }

        virtual int GetScrollLineSize() const
        {
            return m_nLineHeight;
        }

        virtual int Item2Position(HTREEITEM hItem) const
        {
            if(!_IsItemVisible(hItem))
            {
                SASSERT(FALSE);
                return -1;
            }

            int nRet = 0;
            //获得父节点开始位置
            HTREEITEM hParent = m_adapter->GetParentItem(hItem);
            if(hParent!=ITvAdapter::ITEM_NULL && hParent != ITvAdapter::ITEM_ROOT)
            {
                nRet = Item2Position(hParent);
                //越过父节点
                nRet += GetItemHeight(hParent);
            }
            //越过前面兄弟结点
            nRet += _GetItemOffset(hItem);

            return nRet;
        }

        virtual HTREEITEM Position2Item(int position) const
        {
            return _Position2Item(position,ITvAdapter::ITEM_ROOT,0);
        }

        virtual void SetItemWidth(HTREEITEM hItem,int nWidth)
        {
            int nOldWidth = GetItemWidth(hItem);
            if(nOldWidth == nWidth) return;
            int nOldBranchWidth = _GetItemVisibleWidth(hItem);
            _SetItemWidth(hItem,nWidth);
            int nNewBranchWidth = _GetItemVisibleWidth(hItem);
            if(nOldBranchWidth == nNewBranchWidth) return;
            _UpdateBranchWidth(hItem,nOldBranchWidth,nNewBranchWidth);
        }

        virtual void SetItemHeight(HTREEITEM hItem,int nHeight)
        {
            int nOldHeight = GetItemHeight(hItem);
            _SetItemHeight(hItem,nHeight);
            if(nOldHeight != nHeight)
            {
                _UpdateBranchHeight(hItem,nHeight-nOldHeight);
                _UpdateSiblingsOffset(hItem);
            }
        }

        virtual void ExpendItem(HTREEITEM hItem)
        {
            if(IsItemExpanded(hItem)) return;
            SetItemExpanded(hItem,TRUE);
            _UpdateSiblingsOffset(hItem);
        }

        virtual void CollapseItem(HTREEITEM hItem)
        {
            if(!IsItemExpanded(hItem)) return;
            SetItemExpanded(hItem,FALSE);
            _UpdateSiblingsOffset(hItem);        
        }

        virtual HTREEITEM GetNextVisibleItem(HTREEITEM hItem)
        {
            SASSERT(_IsItemVisible(hItem));
            if(IsItemExpanded(hItem))
            {
                HTREEITEM hChild = m_adapter->GetFirstChildItem(hItem);
                if(hChild!=ITvAdapter::ITEM_NULL) return hChild;
            }

            HTREEITEM hParent = hItem;
            while(hParent!=ITvAdapter::ITEM_NULL && hParent!=ITvAdapter::ITEM_ROOT)
            {
                HTREEITEM hRet=m_adapter->GetNextSiblingItem(hParent);
                if(hRet) return hRet;
                hParent=m_adapter->GetParentItem(hParent);
            }
            return ITvAdapter::ITEM_NULL;
        }

        virtual BOOL IsItemExpanded(HTREEITEM hItem) const
        {
            return (BOOL)m_adapter->GetItemDataByIndex(hItem,ITvAdapter::DATA_INDEX_ITEM_EXPANDED);
        }

        virtual void SetItemExpanded(HTREEITEM hItem,BOOL bExpended)
        {
            m_adapter->SetItemDataByIndex(hItem,ITvAdapter::DATA_INDEX_ITEM_EXPANDED,bExpended);
        }

        virtual int GetItemWidth(HTREEITEM hItem) const
        {
            return (int)m_adapter->GetItemDataByIndex(hItem,ITvAdapter::DATA_INDEX_ITEM_WIDTH);
        }
        
        virtual int GetItemHeight(HTREEITEM hItem) const
        {
            return (int)m_adapter->GetItemDataByIndex(hItem,ITvAdapter::DATA_INDEX_ITEM_HEIGHT);
        }

        virtual int GetItemIndent(HTREEITEM hItem) const
        {
            int nRet = 0;
            for(;;)
            {
                hItem = m_adapter->GetParentItem(hItem);
                if(hItem == ITvAdapter::ITEM_ROOT) break;
                nRet += m_nIndent;
            }
            return nRet;
        }
    protected:
        //更新hItem所在的父窗口中分枝宽度数据
        //hItem:显示宽度发生变化的节点，可以是节点本身宽度变化，也可能是子节点宽度发生了变化
        //nOldWidth：原显示宽度
        //nNewWidth: 新显示宽度
        void _UpdateBranchWidth(HTREEITEM hItem,int nOldWidth,int nNewWidth)
        {
            HTREEITEM hParent = m_adapter->GetParentItem(hItem);
            if(hParent == ITvAdapter::ITEM_NULL)
                return;
            int nCurBranchWidth = _GetBranchWidth(hParent);

            int nIndent = hParent==ITvAdapter::ITEM_ROOT?0:m_nIndent;
            if(nCurBranchWidth != nOldWidth+nIndent) 
            {//父节点的宽度不是由当前结点控制的
                if(nCurBranchWidth < nNewWidth+nIndent)
                {//新宽度扩展了父节点的显示宽度
                    _SetBranchWidth(hParent,nNewWidth + nIndent);
                    if(IsItemExpanded(hParent)) _UpdateBranchWidth(hParent,nCurBranchWidth,nNewWidth + nIndent);
                }
            }else
            {//父节点的宽度正好是由hItem的显示宽度
                int nNewBranchWidth;
                if(nNewWidth>nOldWidth)
                {
                    nNewBranchWidth = nNewWidth + nIndent;
                }else
                {
                    HTREEITEM hSib = m_adapter->GetFirstChildItem(hParent);
                    nNewBranchWidth = 0;
                    while(hSib!=ITvAdapter::ITEM_NULL)
                    {
                        nNewBranchWidth = max(nNewBranchWidth,_GetItemVisibleWidth(hSib));
                        hSib = m_adapter->GetNextSiblingItem(hSib);
                    }
                    nNewBranchWidth += nIndent;
                }
                _SetBranchWidth(hParent,nNewBranchWidth);
                if(IsItemExpanded(hParent)) _UpdateBranchWidth(hParent,nCurBranchWidth,nNewBranchWidth);
            }
        }

        int _GetBranchWidth(HTREEITEM hBranch) const
        {
            return m_adapter->GetItemDataByIndex(hBranch,ITvAdapter::DATA_INDEX_BRANCH_WIDTH);
        }

        void _SetBranchWidth(HTREEITEM hBranch,int nWidth)
        {
            m_adapter->SetItemDataByIndex(hBranch,ITvAdapter::DATA_INDEX_BRANCH_WIDTH,nWidth);
        }

        void _SetItemWidth(HTREEITEM hItem,int nWidth)
        {
            m_adapter->SetItemDataByIndex(hItem,ITvAdapter::DATA_INDEX_ITEM_WIDTH,nWidth);
        }


        int _GetBranchHeight(HTREEITEM hItem) const
        {
            return (int)m_adapter->GetItemDataByIndex(hItem,ITvAdapter::DATA_INDEX_BRANCH_HEIGHT);
        }

        void _SetBranchHeight(HTREEITEM hItem ,int nHeight)
        {
            m_adapter->SetItemDataByIndex(hItem,ITvAdapter::DATA_INDEX_BRANCH_HEIGHT,nHeight);
        }

        void _UpdateBranchHeight(HTREEITEM hItem,int nDiff)
        {
            HTREEITEM hParent = m_adapter->GetParentItem(hItem);
            while(hParent != ITvAdapter::ITEM_NULL)
            {
                int nBranchHeight = _GetBranchHeight(hParent);
                _SetBranchHeight(hParent,nBranchHeight+nDiff);
                hParent = m_adapter->GetParentItem(hParent);
            }
        }

        //向后更新兄弟结点的偏移量
        void _UpdateSiblingsOffset(HTREEITEM hItem)
        {

            int nOffset = _GetItemOffset(hItem);
            nOffset += _GetItemVisibleHeight(hItem);

            HTREEITEM hSib = m_adapter->GetNextSiblingItem(hItem);
            while(hSib != ITvAdapter::ITEM_NULL)
            {
                _SetItemOffset(hSib,nOffset);
                nOffset += _GetItemVisibleHeight(hSib);
                hSib = m_adapter->GetNextSiblingItem(hSib);
            }

            //注意更新各级父节点的偏移量
            HTREEITEM hParent = m_adapter->GetParentItem(hItem);
            if(hParent != ITvAdapter::ITEM_NULL && hParent != ITvAdapter::ITEM_ROOT)
            {
                _UpdateSiblingsOffset(hParent);
            }
        }

        int _GetItemOffset(HTREEITEM hItem) const
        {
            return (int)m_adapter->GetItemDataByIndex(hItem,ITvAdapter::DATA_INDEX_ITEM_OFFSET);
        }

        void _SetItemOffset(HTREEITEM hItem, int nOffset)
        {
            m_adapter->SetItemDataByIndex(hItem,ITvAdapter::DATA_INDEX_ITEM_OFFSET,nOffset);
        }

        void _SetItemHeight(HTREEITEM hItem,int nHeight)
        {
            m_adapter->SetItemDataByIndex(hItem,ITvAdapter::DATA_INDEX_ITEM_HEIGHT,nHeight);
        }


        int _GetItemVisibleHeight(HTREEITEM hItem) const
        {
            int nRet = GetItemHeight(hItem);
            if(IsItemExpanded(hItem)) nRet += _GetBranchHeight(hItem);
            return nRet;
        }

        int _GetItemVisibleWidth(HTREEITEM hItem) const
        {
            int nRet = GetItemWidth(hItem);
            if(m_adapter->GetFirstChildItem(hItem)!=ITvAdapter::ITEM_NULL)
            {
                int nIndent = m_adapter->GetParentItem(hItem) == ITvAdapter::ITEM_ROOT?0:m_nIndent;
                nRet = max(nRet,_GetBranchWidth(hItem)+nIndent);
            }
            return nRet;
        }

        HTREEITEM _Position2Item(int position,HTREEITEM hParent,int nParentPosition) const
        {
            if(position<nParentPosition || position>=(nParentPosition+_GetItemVisibleHeight(hParent)))
                return ITvAdapter::ITEM_NULL;

            int nItemHeight = GetItemHeight(hParent);
            int nPos = nParentPosition + nItemHeight;

            if(nPos>position) return hParent;

            SASSERT(IsItemExpanded(hParent));

            int nParentBranchHeight = _GetBranchHeight(hParent);

            if(position-nPos < nParentBranchHeight/2)
            {//从first开始查找
                HTREEITEM hItem = m_adapter->GetFirstChildItem(hParent);
                while(hItem)
                {
                    int nBranchHeight = _GetItemVisibleHeight(hItem);
                    if(nPos + nBranchHeight > position)
                    {
                        return _Position2Item(position,hItem,nPos);
                    }
                    nPos += nBranchHeight;
                    hItem = m_adapter->GetNextSiblingItem(hItem);
                }
            }else
            {//从last开始查找
                nPos += nParentBranchHeight;

                HTREEITEM hItem = m_adapter->GetLastChildItem(hParent);
                while(hItem)
                {
                    int nBranchHeight = _GetItemVisibleHeight(hItem);
                    nPos -= nBranchHeight;
                    if(nPos <= position)
                    {
                        return _Position2Item(position,hItem,nPos);
                    }
                    hItem = m_adapter->GetPrevSiblingItem(hItem);
                }

            }

            SASSERT(FALSE);//不应该走到这里来
            return ITvAdapter::ITEM_NULL;
        }

        BOOL _IsItemVisible(HTREEITEM hItem) const
        {
            HTREEITEM hParent = m_adapter->GetParentItem(hItem);
            while(hParent != ITvAdapter::ITEM_NULL)
            {
                if(!IsItemExpanded(hParent)) return FALSE;
                hParent = m_adapter->GetParentItem(hParent);
            }
            return TRUE;
        }

        void _InitTreeView(HTREEITEM hItem)
        {
            if(hItem != ITvAdapter::ITEM_ROOT)
            {
                _SetItemHeight(hItem,m_szDef.cy);
                _SetItemWidth(hItem,m_szDef.cx);
            }else
            {
                _SetItemHeight(hItem,0);
                _SetItemWidth(hItem,0);
            }
            if(m_adapter->GetFirstChildItem(hItem)!=ITvAdapter::ITEM_NULL)
            {//有子节点
                HTREEITEM hChild = m_adapter->GetFirstChildItem(hItem);
                int nBranchHeight = 0;
                while(hChild != ITvAdapter::ITEM_NULL)
                {
                    //设置偏移
                    _SetItemOffset(hChild,nBranchHeight);
                    _InitTreeView(hChild);
                    nBranchHeight += _GetItemVisibleHeight(hChild);
                    hChild = m_adapter->GetNextSiblingItem(hChild);
                }
                _SetBranchHeight(hItem,nBranchHeight);
                //自动展开
                SetItemExpanded(hItem,TRUE);
                //设置默认宽度
                _SetBranchWidth(hItem,m_szDef.cx + m_nIndent);
            }else
            {//无子节点
                _SetBranchHeight(hItem,0);
                _SetBranchWidth(hItem,0);
            }
        }

        CAutoRefPtr<ITvAdapter> m_adapter;
        int                     m_nLineHeight;
        int                     m_nIndent;
        CSize                   m_szDef;
    };

	//////////////////////////////////////////////////////////////////////////
	STreeView::STreeView()
		: m_itemCapture(NULL)
		, m_pHoverItem(NULL)
		, m_hSelected(ITvAdapter::ITEM_NULL)
		, m_pVisibleMap(new VISIBLEITEMSMAP)
	{
		m_bFocusable = TRUE;
		m_evtSet.addEvent(EVENTID(EventTVSelChanged));
		m_observer.Attach(new STreeViewDataSetObserver(this));
		m_tvItemLocator.Attach(new STreeViewItemLocator);
	}

	STreeView::~STreeView()
	{
	    delete m_pVisibleMap;
	}
	
	BOOL STreeView::SetAdapter( ITvAdapter * adapter )
	{
		if (m_adapter)
		{
			m_adapter->unregisterDataSetObserver(m_observer);
		}
		m_adapter = adapter;
		if (m_adapter)
		{
			m_adapter->registerDataSetObserver(m_observer);
			
            //free all itemPanels in recycle
            for(size_t i=0;i<m_itemRecycle.GetCount();i++)
            {
                SList<SItemPanel*> *lstItemPanels = m_itemRecycle.GetAt(i);
                SPOSITION pos = lstItemPanels->GetHeadPosition();
                while(pos)
                {
                    SItemPanel * pItemPanel = lstItemPanels->GetNext(pos);
                    pItemPanel->DestroyWindow();
                }
                delete lstItemPanels;
            }
            m_itemRecycle.RemoveAll();

            //free all visible itemPanels
            SPOSITION pos=m_pVisibleMap->GetStartPosition();
            while(pos)
            {
                ItemInfo ii = m_pVisibleMap->GetNext(pos)->m_value;
                ii.pItem->DestroyWindow();
            }
            m_pVisibleMap->RemoveAll();
		}
		
		if(m_tvItemLocator)
		{
		    m_tvItemLocator->SetAdapter(adapter);
		    
            for(int i=0;i<m_adapter->getViewTypeCount();i++)
            {
                m_itemRecycle.Add(new SList<SItemPanel*>());
            }

		}
		onDataSetChanged(ITvAdapter::ITEM_ROOT);
		return TRUE;
	}
	void STreeView::onDataSetInvalidated(HTREEITEM hItem)
	{
		onDataSetChanged(hItem);
	}
	
	BOOL STreeView::CreateChildren(pugi::xml_node xmlNode) {
		pugi::xml_node xmlTemplate = xmlNode.child(L"template");
		if (xmlTemplate)
		{
			m_xmlTemplate.append_copy(xmlTemplate);
		}
		return TRUE;
	}
	
	void STreeView::OnPaint( IRenderTarget * pRT )
	{
		SPainter painter;
		BeforePaint(pRT, painter);
		
        CRect rcClient;
        GetClientRect(&rcClient);
        int width = rcClient.Width();
        pRT->PushClipRect(&rcClient, RGN_AND);

        CRect rcClip, rcInter;
        pRT->GetClipBox(&rcClip);

		CPoint pt(0,-1);
		for(SPOSITION pos = m_visible_items.GetHeadPosition();pos;)
		{
		    ItemInfo ii = m_visible_items.GetNext(pos);
		    HTREEITEM hItem = (HTREEITEM)ii.pItem->GetItemIndex();
		    if(pt.y == -1)
		    {
		        pt.y = m_tvItemLocator->Item2Position(hItem)-m_siVer.nPos;
		    }
		    CSize szItem(m_tvItemLocator->GetItemWidth(hItem),m_tvItemLocator->GetItemWidth(hItem));
		    
		    pt.x = m_tvItemLocator->GetItemIndent(hItem) - m_siHoz.nPos;
		    
		    CRect rcItem(pt,szItem);
		    rcItem.OffsetRect(rcClient.TopLeft());
		    if(!(rcItem & rcClip).IsRectEmpty())
		    {//draw the item
		        ii.pItem->Draw(pRT,rcItem);
		    }
            pt.y += m_tvItemLocator->GetItemHeight(hItem);
		}
		
        pRT->PopClip();

		AfterPaint(pRT, painter);
	}

	void STreeView::OnSize( UINT nType, CSize size )
	{
		__super::OnSize(nType, size);
		if(!m_adapter) return;
		UpdateScrollBar();
		UpdateVisibleItems();
	}

	void STreeView::OnDestroy()
	{
		//destroy all itempanel
		SPOSITION pos = m_visible_items.GetHeadPosition();
		while(pos)
		{
			ItemInfo ii = m_visible_items.GetNext(pos);
			ii.pItem->Release();
		}
		m_visible_items.RemoveAll();

        for(int i=0;i<(int)m_itemRecycle.GetCount();i++)
        {
            SList<SItemPanel*> *pLstTypeItems = m_itemRecycle[i];
            SPOSITION pos = pLstTypeItems->GetHeadPosition();
            while(pos)
            {
                SItemPanel *pItem = pLstTypeItems->GetNext(pos);
                pItem->Release();
            }
            delete pLstTypeItems;
        }
        m_itemRecycle.RemoveAll();

		__super::OnDestroy();
	}

	void STreeView::EnsureVisible(HTREEITEM hItem)
	{

	}

	LRESULT STreeView::OnKeyEvent(UINT uMsg,WPARAM wParam,LPARAM lParam)
	{
	    return 0;
	}

	void STreeView::UpdateScrollBar()
	{
        CSize szView;
        szView.cx = m_tvItemLocator->GetTotalWidth();
        szView.cy = m_tvItemLocator->GetTotalHeight();

        CRect rcClient;
        SWindow::GetClientRect(&rcClient);//不计算滚动条大小
        CSize size = rcClient.Size();
        //  关闭滚动条
        m_wBarVisible = SSB_NULL;

        if (size.cy<szView.cy || (size.cy<szView.cy+m_nSbWid && size.cx<szView.cx))
        {
            //  需要纵向滚动条
            m_wBarVisible |= SSB_VERT;
            m_siVer.nMin  = 0;
            m_siVer.nMax  = szView.cy-1;
            m_siVer.nPage = rcClient.Height();

            if (size.cx-m_nSbWid < szView.cx)
            {
                //  需要横向滚动条
                m_wBarVisible |= SSB_HORZ;
                m_siVer.nPage=size.cy-m_nSbWid > 0 ? size.cy-m_nSbWid : 0;//注意同时调整纵向滚动条page信息

                m_siHoz.nMin  = 0;
                m_siHoz.nMax  = szView.cx-1;
                m_siHoz.nPage = (size.cx-m_nSbWid) > 0 ? (size.cx-m_nSbWid) : 0;
            }
            else
            {
                //  不需要横向滚动条
                m_siHoz.nPage = size.cx;
                m_siHoz.nMin  = 0;
                m_siHoz.nMax  = m_siHoz.nPage-1;
                m_siHoz.nPos  = 0;
            }
        }
        else
        {
            //  不需要纵向滚动条
            m_siVer.nPage = size.cy;
            m_siVer.nMin  = 0;
            m_siVer.nMax  = size.cy-1;
            m_siVer.nPos  = 0;

            if (size.cx < szView.cx)
            {
                //  需要横向滚动条
                m_wBarVisible |= SSB_HORZ;
                m_siHoz.nMin  = 0;
                m_siHoz.nMax  = szView.cx-1;
                m_siHoz.nPage = size.cx;
            }
            else
            {
                //  不需要横向滚动条
                m_siHoz.nPage = size.cx;
                m_siHoz.nMin  = 0;
                m_siHoz.nMax  = m_siHoz.nPage-1;
                m_siHoz.nPos  = 0;
            }
        }

        //  根据需要调整原点位置
        if (HasScrollBar(FALSE) && m_siHoz.nPos+(int)m_siHoz.nPage>szView.cx)
        {
            m_siHoz.nPos = szView.cx-m_siHoz.nPage;
        }

        if (HasScrollBar(TRUE) && m_siVer.nPos +(int)m_siVer.nPage>szView.cy)
        {
            m_siVer.nPos = szView.cy-m_siVer.nPage;
        }

        SetScrollPos(TRUE, m_siVer.nPos, TRUE);
        SetScrollPos(FALSE, m_siHoz.nPos, TRUE);

        //  重新计算客户区及非客户区
        SSendMessage(WM_NCCALCSIZE);

        Invalidate();
	}

	void STreeView::UpdateVisibleItems()
	{
        if(!m_adapter) return;
        HTREEITEM hItem = m_tvItemLocator->Position2Item(m_siVer.nPos);
        if(hItem == ITvAdapter::ITEM_NULL) return;
        
        CSize szOldView;
        szOldView.cx = m_tvItemLocator->GetTotalWidth();
        szOldView.cy = m_tvItemLocator->GetTotalHeight();
        
        VISIBLEITEMSMAP *pMapOld = m_pVisibleMap;
        m_pVisibleMap = new VISIBLEITEMSMAP;
        
        CRect rcClient = GetClientRect();
        CRect rcContainer=rcClient;
        rcContainer.MoveToXY(0,0);
        rcContainer.bottom=10000;
        
        int nOffset = m_tvItemLocator->Item2Position(hItem) - m_siVer.nPos;
        
        m_visible_items.RemoveAll();
        while(hItem != ITvAdapter::ITEM_NULL)
        {
            VISIBLEITEMSMAP::CPair *pFind = pMapOld->Lookup(hItem);
            ItemInfo ii;
            if(pFind)
            {//re use the previous item;
                ii = pFind->m_value;
                pMapOld->RemoveKey(hItem);
            }else
            {
                ii.nType = m_adapter->getViewType(hItem);
                SList<SItemPanel *> *lstRecycle = m_itemRecycle.GetAt(ii.nType);
                if(lstRecycle->IsEmpty())
                {//创建一个新的列表项
                    ii.pItem = SItemPanel::Create(this,pugi::xml_node(),this);
                }else
                {
                    ii.pItem = lstRecycle->RemoveHead();
                }
                ii.pItem->SetItemIndex(hItem);
            }
            m_pVisibleMap->SetAt(hItem,ii);
            
            if(hItem == m_hSelected)
                ii.pItem->ModifyItemState(WndState_Check,0);
            else
                ii.pItem->ModifyItemState(0,WndState_Check);
                
            if(m_pHoverItem && hItem == (HTREEITEM)m_pHoverItem->GetItemIndex())
                ii.pItem->ModifyItemState(WndState_Hover,0);
            else
                ii.pItem->ModifyItemState(0,WndState_Hover);
                
            m_adapter->getView(hItem,ii.pItem,m_xmlTemplate.first_child());
            
            rcContainer.left = m_tvItemLocator->GetItemIndent(hItem);
            CSize szItem = ii.pItem->GetDesiredSize(&rcContainer);
            ii.pItem->Move(CRect(0,0,szItem.cx,szItem.cy));
            m_tvItemLocator->SetItemWidth(hItem,szItem.cx);
            m_tvItemLocator->SetItemHeight(hItem,szItem.cy);
                        
            m_visible_items.AddTail(ii);
            nOffset += szItem.cy;
            if(nOffset>=rcClient.Height())
                break;
            hItem = m_tvItemLocator->GetNextVisibleItem(hItem);
        }

        SPOSITION pos = pMapOld->GetStartPosition();
        while(pos)
        {
            ItemInfo ii = pMapOld->GetNextValue(pos);
            m_itemRecycle[ii.nType]->AddTail(ii.pItem);    
        }
        delete pMapOld;
        
        CSize szNewView;
        szNewView.cx = m_tvItemLocator->GetTotalWidth();
        szNewView.cy = m_tvItemLocator->GetTotalHeight();
        if(szOldView != szNewView)
        {//update scroll range
            UpdateScrollBar();
            UpdateVisibleItems();//根据新的滚动条状态重新记录显示列表项
        }
	}


	void STreeView::OnItemSetCapture( SItemPanel *pItem,BOOL bCapture )
	{
		if(bCapture)
		{
			GetContainer()->OnSetSwndCapture(m_swnd);
			m_itemCapture=pItem;
		}else
		{
			GetContainer()->OnReleaseSwndCapture();
			m_itemCapture=NULL;
		}

	}

	BOOL STreeView::OnItemGetRect( SItemPanel *pItem,CRect &rcItem )
	{
        HTREEITEM hItem = (HTREEITEM)pItem->GetItemIndex();
        int nOffset = m_tvItemLocator->Item2Position(hItem) - m_siVer.nPos;
        rcItem = GetClientRect();
        rcItem.top += nOffset;
        rcItem.bottom = rcItem.top + m_tvItemLocator->GetItemHeight(hItem);
        rcItem.left += m_tvItemLocator->GetItemIndent(hItem) - m_siHoz.nPos;
        rcItem.right = rcItem.left + m_tvItemLocator->GetItemWidth(hItem);
        return TRUE;
	}

	BOOL STreeView::IsItemRedrawDelay()
	{
		return TRUE;
	}

	void STreeView::OnItemRequestRelayout( SItemPanel *pItem )
	{
		  pItem->UpdateChildrenPosition();
	}

	void STreeView::onDataSetChanged(HTREEITEM hItem)
	{
		if (m_adapter == NULL)
		{
			return; 
		}
        if(m_tvItemLocator) m_tvItemLocator->OnDataSetChanged(hItem);

		UpdateScrollBar();
		UpdateVisibleItems();
	}

	void STreeView::OnLButtonDbClick(UINT nFlags,CPoint pt)
	{
	}
	
	void STreeView::OnLButtonDown(UINT nFlags,CPoint pt)
	{
		
	}
	
	LRESULT STreeView::OnMouseEvent( UINT uMsg,WPARAM wParam,LPARAM lParam )
	{
		if(m_adapter == NULL) {
			SetMsgHandled(FALSE);
			return 0;
		}

		LRESULT lRet = 0;
		CPoint pt(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));

		if(m_itemCapture) {
			CRect rcItem = m_itemCapture->GetItemRect();
			pt.Offset(-rcItem.TopLeft());
			lRet = m_itemCapture->DoFrameEvent(uMsg, wParam, MAKELPARAM(pt.x, pt.y));
		}
		else {
			if (m_bFocusable && uMsg == WM_RBUTTONDOWN)
			{
				SetFocus();
			}

			SItemPanel * pHover = HitTest(pt);
			if (pHover != m_pHoverItem)
			{
				SItemPanel * oldHover = m_pHoverItem;
				m_pHoverItem = pHover;
				if (oldHover)
				{
					oldHover->DoFrameEvent(WM_MOUSELEAVE, 0, 0);
					oldHover->InvalidateRect(NULL);
				}
				if (m_pHoverItem)
				{
					m_pHoverItem->DoFrameEvent(WM_MOUSEHOVER, 0, 0);
					m_pHoverItem->InvalidateRect(NULL);
				}
			}
			
			if (m_pHoverItem)
			{
				m_pHoverItem->DoFrameEvent(uMsg, wParam, MAKELPARAM(pt.x, pt.y));
			}
		}

		CPoint pt2(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		if(uMsg == WM_LBUTTONUP)
			__super::OnLButtonUp(wParam,pt2);
		else if(uMsg == WM_RBUTTONDOWN)
			__super::OnRButtonDown(uMsg, pt2);
		return 0;
	}

	void STreeView::RedrawItem(SItemPanel *pItem)
	{
		pItem->InvalidateRect(NULL);
	}

	void STreeView::OnMouseLeave()
	{
		if(m_pHoverItem)
		{
			m_pHoverItem->DoFrameEvent(WM_MOUSELEAVE,0,0);
			m_pHoverItem = NULL;
		}
	}

	BOOL STreeView::OnMouseWheel( UINT nFlags, short zDelta, CPoint pt )
	{
        SItemPanel *pSelItem = GetItemPanel(m_hSelected);
        if(pSelItem)
        {
            CRect rcItem = pSelItem->GetItemRect();
            CPoint pt2=pt-rcItem.TopLeft();
            if(pSelItem->DoFrameEvent(WM_MOUSEWHEEL,MAKEWPARAM(nFlags,zDelta),MAKELPARAM(pt2.x,pt2.y)))
                return TRUE;
        }
		return __super::OnMouseWheel(nFlags, zDelta, pt);
	}
	
	void STreeView::OnKillFocus()
	{
		__super::OnKillFocus();
		SItemPanel * itemPanel = GetItemPanel(m_hSelected);
		if (itemPanel) itemPanel->GetFocusManager()->StoreFocusedView();
	}

	void STreeView::OnSetFocus()
	{
		__super::OnSetFocus();
		SItemPanel * itemPanel = GetItemPanel(m_hSelected);
		if (itemPanel)
		{
			itemPanel->GetFocusManager()->RestoreFocusedView();
		}
		
	}
	BOOL STreeView::OnScroll( BOOL bVertical,UINT uCode,int nPos )
	{
		int nOldPos = m_siVer.nPos;
		__super::OnScroll(bVertical, uCode, nPos);                      
		int nNewPos = m_siVer.nPos;
		if(nOldPos != nNewPos)
		{
			UpdateVisibleItems();

			//加速滚动时UI的刷新
			if (uCode==SB_THUMBTRACK)
				ScrollUpdate();

		}
		return TRUE;
	}

	int STreeView::GetScrollLineSize( BOOL bVertical )
	{
		return m_tvItemLocator->GetScrollLineSize();
	}

    SItemPanel * STreeView::GetItemPanel(HTREEITEM hItem)
    {
        VISIBLEITEMSMAP::CPair *pNode = m_pVisibleMap->Lookup(hItem);
        if(!pNode) return NULL;
        return pNode->m_value.pItem;
    }

    SItemPanel * STreeView::HitTest(CPoint & pt)
    {
        SPOSITION pos = m_visible_items.GetHeadPosition();
        while(pos)
        {
            ItemInfo ii = m_visible_items.GetNext(pos);
            CRect rcItem = ii.pItem->GetItemRect();
            if(rcItem.PtInRect(pt)) 
            {
                pt-=rcItem.TopLeft();
                return ii.pItem;
            }
        }
        return NULL;
    }
    
	BOOL STreeView::FireEvent(EventArgs &evt)
	{
// 		if(evt.GetID()==EventOfPanel::EventID)
// 		{
// 			EventOfPanel *pEvt = (EventOfPanel *)&evt;
// 			if(pEvt->pOrgEvt->GetID()==EVT_CMD 
// 				&& wcscmp(pEvt->pOrgEvt->nameFrom , NAME_SWITCH)==0)
// 			{
// 				int index =pEvt->pPanel->GetItemIndex();
// 				Expand(index,TVE_TOGGLE);
// 				return TRUE;
// 			}
// 		}
		return __super::FireEvent(evt);
	}

	BOOL STreeView::Expand(HTREEITEM hItem , UINT nCode)
	{
// 		if (m_current_list.GetCount() < nItem) return FALSE;
// 		SPOSITION pos = m_current_list.FindIndex(nItem);
// 		HSTREEITEM loc = m_current_list.GetAt(pos);
// 		int itemTreeState = m_node_state[loc];
// 		BOOL bRet = FALSE;
// 
// 		if (itemTreeState == TS_Single)
// 		{
// 			return FALSE;
// 		}
// 
// 		if(itemTreeState == TS_Expanded)
// 		{
// 			if(nCode==TVE_COLLAPSE || nCode == TVE_TOGGLE)
// 			{
// 				//collapse it
// 				m_node_state[loc]= TS_Collapsed;
// 				CurrentTree2List();
// 				UpdateScrollBar();
// 				UpdateVisibleItems();
// 				bRet = TRUE;
// 			}
// 		}
// 		else
// 		{
// 			if(nCode==TVE_TOGGLE || nCode == TVE_EXPAND)
// 			{//expand it
// 				m_node_state[loc]= TS_Expanded;
// 
// 				CurrentTree2List();
// 				UpdateScrollBar();
// 				UpdateVisibleItems();
// 				bRet = TRUE;
// 			}
// 		}

// 		return bRet;
    return FALSE;
	}

}
