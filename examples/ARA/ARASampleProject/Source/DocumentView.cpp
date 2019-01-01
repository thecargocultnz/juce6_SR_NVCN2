#include "DocumentView.h"

#include "RegionSequenceView.h"
#include "PlaybackRegionView.h"
#include "RulersView.h"

constexpr int kRulersViewHeight = 3*20;
constexpr int kTrackHeaderWidth = 120;
constexpr int kTrackHeight = 80;
constexpr int kStatusBarHeight = 20;

//==============================================================================
DocumentView::DocumentView (AudioProcessor& p)
: AudioProcessorEditor (&p),
AudioProcessorEditorARAExtension (&p),
playbackRegionsViewPort (*this),
playheadView (*this),
positionInfoPtr (nullptr)
{
    playheadView.setAlwaysOnTop (true);
    playbackRegionsView.addAndMakeVisible (playheadView);
    
    playbackRegionsViewPort.setScrollBarsShown (true, true, false, false);
    playbackRegionsViewPort.setViewedComponent (&playbackRegionsView, false);
    addAndMakeVisible (playbackRegionsViewPort);
    
    trackHeadersViewPort.setScrollBarsShown (false, false, false, false);
    trackHeadersViewPort.setViewedComponent (&trackHeadersView, false);
    addAndMakeVisible (trackHeadersViewPort);
    
    zoomInButton.setButtonText("+");
    zoomOutButton.setButtonText("-");
    constexpr double zoomStepFactor = 1.5;
    zoomInButton.onClick = [this, zoomStepFactor]
    {
        pixelsPerSecond *= zoomStepFactor;
        resized();
    };
    zoomOutButton.onClick = [this, zoomStepFactor]
    {
        pixelsPerSecond /= zoomStepFactor;
        resized();
    };
    addAndMakeVisible (zoomInButton);
    addAndMakeVisible (zoomOutButton);
    
    followPlayheadToggleButton.setButtonText ("Viewport follows playhead");
    followPlayheadToggleButton.setToggleState (true, dontSendNotification);
    addAndMakeVisible (followPlayheadToggleButton);
    
    if (isARAEditorView())
    {
        getARAEditorView()->addListener (this);
        getARADocumentController()->getDocument<ARADocument>()->addListener (this);
        
        rulersView.reset (new RulersView (*this));
        rulersViewPort.setScrollBarsShown (false, false, false, false);
        rulersViewPort.setViewedComponent (rulersView.get(), false);
        addAndMakeVisible (rulersViewPort);
    }
    startTimerHz (60);
}

DocumentView::~DocumentView()
{
    if (isARAEditorView())
    {
        getARADocumentController()->getDocument<ARADocument>()->removeListener (this);
        getARAEditorView()->removeListener (this);
    }
}

//==============================================================================
int DocumentView::getPlaybackRegionsViewsXForTime (double time) const
{
    return roundToInt ((time - startTime) / (endTime - startTime) * playbackRegionsView.getWidth());
}

double DocumentView::getPlaybackRegionsViewsTimeForX (int x) const
{
    return startTime + ((double) x / (double) playbackRegionsView.getWidth()) * (endTime - startTime);
}

//==============================================================================
void DocumentView::paint (Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
    
    if (! isARAEditorView())
    {
        g.setColour (Colours::white);
        g.setFont (20.0f);
        g.drawFittedText ("Non ARA Instance. Please re-open as ARA2!", getLocalBounds(), Justification::centred, 1);
    }
    else
    {
        if (regionSequenceViewsAreInvalid)
            rebuildRegionSequenceViews();
    }
}

void DocumentView::resized()
{
    // store visible playhead postion (in main view coordinates)
    int previousPlayheadX = getPlaybackRegionsViewsXForTime (playheadTimePosition) - playbackRegionsViewPort.getViewPosition().getX();
    
    // calculate maximum visible time range
    if (regionSequenceViews.isEmpty())
    {
        startTime = 0.0;
        endTime = 0.0;
    }
    else
    {
        startTime = std::numeric_limits<double>::max();
        endTime = std::numeric_limits<double>::lowest();
        for (auto v : regionSequenceViews)
        {
            auto sequenceTimeRange = v->getTimeRange();
            startTime = jmin (startTime, sequenceTimeRange.getStart());
            endTime = jmax (endTime, sequenceTimeRange.getEnd());
        }
    }
    
    // make sure we can see at least 1 second
    constexpr double minDuration = 1.0;
    double duration = endTime - startTime;
    if (duration < minDuration)
    {
        startTime -= (minDuration - duration) / 2.0;
        endTime = startTime + minDuration;
    }
    
    // add a second left and right so that regions will not directly hit the end of the view
    constexpr double borderTime = 1.0;
    startTime -= borderTime;
    endTime += borderTime;
    
    // max zoom 1px : 1sample (this is a naive assumption as audio can be in different sample rate)
    double maxPixelsPerSecond = jmax (processor.getSampleRate(), 300.0);
    
    // min zoom covers entire view range
    double minPixelsPerSecond = (getWidth() - kTrackHeaderWidth - rulersViewPort.getScrollBarThickness()) / (endTime - startTime);
    
    // enforce zoom in/out limits, update zoom buttons
    pixelsPerSecond = jmax (minPixelsPerSecond, jmin (pixelsPerSecond, maxPixelsPerSecond));
    zoomOutButton.setEnabled (pixelsPerSecond > minPixelsPerSecond);
    zoomInButton.setEnabled (pixelsPerSecond < maxPixelsPerSecond);
    
    // update sizes and positions of all views
    playbackRegionsViewPort.setBounds (kTrackHeaderWidth, kRulersViewHeight, getWidth() - kTrackHeaderWidth, getHeight() - kRulersViewHeight - kStatusBarHeight);
    playbackRegionsView.setBounds (0, 0, roundToInt ((endTime - startTime) * pixelsPerSecond), jmax (kTrackHeight * regionSequenceViews.size(), playbackRegionsViewPort.getHeight() - playbackRegionsViewPort.getScrollBarThickness()));
    pixelsPerSecond = playbackRegionsView.getWidth() / (endTime - startTime);       // prevent potential rounding issues
    
    trackHeadersViewPort.setBounds (0, kRulersViewHeight, kTrackHeaderWidth, playbackRegionsViewPort.getMaximumVisibleHeight());
    trackHeadersView.setBounds (0, 0, kTrackHeaderWidth, playbackRegionsView.getHeight());
    
    if (rulersView != nullptr)
    {
        rulersViewPort.setBounds (kTrackHeaderWidth, 0, playbackRegionsViewPort.getMaximumVisibleWidth(), kRulersViewHeight);
        rulersView->setBounds (0, 0, playbackRegionsView.getWidth(), kRulersViewHeight);
    }
    
    int y = 0;
    for (auto v : regionSequenceViews)
    {
        v->setRegionsViewBoundsByYRange (y, kTrackHeight);
        y += kTrackHeight;
    }
    
    playheadView.setBounds (playbackRegionsView.getBounds());
    
    zoomInButton.setBounds (getWidth() - kStatusBarHeight, getHeight() - kStatusBarHeight, kStatusBarHeight, kStatusBarHeight);
    zoomOutButton.setBounds (zoomInButton.getBounds().translated (-kStatusBarHeight, 0));
    followPlayheadToggleButton.setBounds (0, zoomInButton.getY(), 200, kStatusBarHeight);
    
    // keep viewport position relative to playhead
    // TODO JUCE_ARA if playhead is not visible in new position, we should rather keep the
    //               left or right border stable, depending on which side the playhead is.
    auto relativeViewportPosition = playbackRegionsViewPort.getViewPosition();
    relativeViewportPosition.setX (getPlaybackRegionsViewsXForTime (playheadTimePosition) - previousPlayheadX);
    playbackRegionsViewPort.setViewPosition (relativeViewportPosition);
    rulersViewPort.setViewPosition (relativeViewportPosition.getX(), 0);
}

void DocumentView::rebuildRegionSequenceViews()
{
    regionSequenceViews.clear();
    
    for (auto regionSequence : getARADocumentController()->getDocument()->getRegionSequences<ARARegionSequence>())
    {
        if (!showOnlySelectedRegionSequence && ! ARA::contains (getARAEditorView()->getHiddenRegionSequences(), regionSequence))
        {
            regionSequenceViews.add (getViewForRegionSequence(regionSequence));
        }
        else
        {
            auto selectedSequences = getARAEditorView()->getViewSelection().getRegionSequences();
            if (ARA::contains (selectedSequences, regionSequence))
            {
                regionSequenceViews.add (getViewForRegionSequence(regionSequence));
            }
        }
    }
    
    resized();
}

//==============================================================================
void DocumentView::onHideRegionSequences (std::vector<ARARegionSequence*> const& /*regionSequences*/)
{
    rebuildRegionSequenceViews();
}

void DocumentView::didEndEditing (ARADocument* document)
{
    jassert (document == getARADocumentController()->getDocument());
    
    if (regionSequenceViewsAreInvalid)
    {
        rebuildRegionSequenceViews();
        regionSequenceViewsAreInvalid = false;
    }
}

void DocumentView::didReorderRegionSequencesInDocument (ARADocument* document)
{
    jassert (document == getARADocumentController()->getDocument());
    
    invalidateRegionSequenceViews();
}

PlaybackRegionView* DocumentView::getViewForPlaybackRegion (ARAPlaybackRegion* playbackRegion)
{
    return new PlaybackRegionView (*this, playbackRegion);
}

RegionSequenceView* DocumentView::getViewForRegionSequence (ARARegionSequence* regionSequence)
{
    return new RegionSequenceView (*this, regionSequence);
}

Range<double> DocumentView::getVisibleTimeRange() const
{
    double start = getPlaybackRegionsViewsTimeForX (playbackRegionsViewPort.getViewArea().getX());
    double end = getPlaybackRegionsViewsTimeForX (playbackRegionsViewPort.getViewArea().getRight());
    return Range<double> (start, end);
}

void DocumentView::timerCallback()
{
    if (positionInfoPtr == nullptr)
        return;
        
    if (playheadTimePosition != positionInfoPtr->timeInSeconds)
    {
        playheadTimePosition = positionInfoPtr->timeInSeconds;

        if (followPlayheadToggleButton.getToggleState())
        {
            Range<double> visibleRange = getVisibleTimeRange();
            if (playheadTimePosition < visibleRange.getStart() || playheadTimePosition > visibleRange.getEnd())
                playbackRegionsViewPort.setViewPosition (playbackRegionsViewPort.getViewPosition().withX (getPlaybackRegionsViewsXForTime (playheadTimePosition)));
        };

        playheadView.repaint();
    }
}

void DocumentView::setCurrentPositionInfo (const AudioPlayHead::CurrentPositionInfo* curPosInfoPtr)
{
    positionInfoPtr = curPosInfoPtr;
}

//==============================================================================
DocumentView::PlayheadView::PlayheadView (DocumentView& documentView)
: documentView (documentView)
{}

void DocumentView::PlayheadView::paint (juce::Graphics &g)
{
    static constexpr int kPlayheadWidth = 1;
    const int playheadX = documentView.getPlaybackRegionsViewsXForTime (documentView.getPlayheadTimePosition());
    g.setColour (findColour (ScrollBar::ColourIds::thumbColourId));
    g.fillRect (playheadX - kPlayheadWidth / 2, 0, kPlayheadWidth, getHeight());
}

//==============================================================================
// see https://forum.juce.com/t/viewport-scrollbarmoved-mousewheelmoved/20226
void DocumentView::ScrollMasterViewPort::visibleAreaChanged (const Rectangle<int>& newVisibleArea)
{
    Viewport::visibleAreaChanged (newVisibleArea);
    
    documentView.getRulersViewPort().setViewPosition (newVisibleArea.getX(), 0);
    documentView.getTrackHeadersViewPort().setViewPosition (0, newVisibleArea.getY());
}
